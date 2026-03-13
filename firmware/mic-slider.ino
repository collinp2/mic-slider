// mic-slider.ino
// Arduino sketch for a WiFi-controlled stepper-motor microphone slider.
//
// Hardware:
//   - ESP32 (recommended) OR Arduino Uno + ESP8266 WiFi shield
//   - A4988 stepper driver
//   - Two limit switches (left & right)
//
// Libraries (install via Arduino Library Manager):
//   - AccelStepper by Mike McCauley
//   - If using Uno + ESP8266 shield: WiFiEsp by bportaluri
//   - If using ESP32: built-in WiFi library (no install needed)
//
// HTTP endpoints (all GET):
//   /status              → JSON: {"pos":0,"moving":false,"maxSteps":20000}
//   /move?dir=left&steps=200
//   /jog/start?dir=left  → begin continuous movement
//   /jog/stop            → stop immediately
//   /home                → run to left limit switch, zero position
//   /speed?val=800       → set max speed (steps/sec)

#include <AccelStepper.h>
#include "config.h"

// ── Platform-specific WiFi / server includes ──────────────────────────────────
#ifdef ESP32
  #include <WiFi.h>
  #include <WebServer.h>
  WebServer server(HTTP_PORT);
#else
  // Arduino Uno + ESP8266 AT-command shield
  #include <WiFiEsp.h>
  #include <WiFiEspServer.h>
  #include <SoftwareSerial.h>
  SoftwareSerial Serial1(10, 11); // RX, TX to ESP8266 shield
  WiFiEspServer server(HTTP_PORT);
#endif

// ── Stepper ───────────────────────────────────────────────────────────────────
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// ── State ─────────────────────────────────────────────────────────────────────
enum class JogDir { NONE, LEFT, RIGHT };
JogDir jogDir = JogDir::NONE;
bool homing   = false;

// ── Helpers ───────────────────────────────────────────────────────────────────

bool limitLeft()  { return digitalRead(LIMIT_LEFT)  == LOW; }
bool limitRight() { return digitalRead(LIMIT_RIGHT) == LOW; }

void enableMotor()  { digitalWrite(ENABLE_PIN, LOW); }
void disableMotor() { digitalWrite(ENABLE_PIN, HIGH); }

void stopMotor() {
  stepper.stop();
  stepper.runToPosition(); // finish deceleration in place (blocking — brief)
  jogDir  = JogDir::NONE;
  homing  = false;
  disableMotor();
}

// ── HTTP response helpers ─────────────────────────────────────────────────────

String getParam(String query, String key) {
  // Extract value of `key` from a query string like "dir=left&steps=200"
  int start = query.indexOf(key + "=");
  if (start == -1) return "";
  start += key.length() + 1;
  int end = query.indexOf("&", start);
  return (end == -1) ? query.substring(start) : query.substring(start, end);
}

String statusJSON() {
  String json = "{\"pos\":";
  json += stepper.currentPosition();
  json += ",\"moving\":";
  json += (stepper.isRunning() ? "true" : "false");
  json += ",\"maxSteps\":";
  json += MAX_STEPS;
  json += ",\"jogDir\":\"";
  json += (jogDir == JogDir::LEFT  ? "left"  :
           jogDir == JogDir::RIGHT ? "right" : "none");
  json += "\",\"homing\":";
  json += (homing ? "true" : "false");
  json += "}";
  return json;
}

// ── Route handlers ────────────────────────────────────────────────────────────

void handleStatus() {
  server.send(200, "application/json", statusJSON());
}

void handleMove() {
  String dir   = getParam(server.uri().substring(server.uri().indexOf('?') + 1), "dir");
  String steps = getParam(server.uri().substring(server.uri().indexOf('?') + 1), "steps");

  if (dir == "" || steps == "") {
    server.send(400, "text/plain", "Missing dir or steps");
    return;
  }

  long n = steps.toInt();
  if (n <= 0) { server.send(400, "text/plain", "Invalid steps"); return; }

  enableMotor();
  homing = false;
  jogDir = JogDir::NONE;

  if (dir == "left")  stepper.move(-n);
  else                stepper.move(n);

  server.send(200, "application/json", statusJSON());
}

void handleJogStart() {
  String uri = server.uri();
  String dir = getParam(uri.substring(uri.indexOf('?') + 1), "dir");

  if (dir == "") { server.send(400, "text/plain", "Missing dir"); return; }

  enableMotor();
  homing = false;

  // Move a very large number of steps — /jog/stop will interrupt.
  if (dir == "left") {
    jogDir = JogDir::LEFT;
    stepper.move(-1000000L);
  } else {
    jogDir = JogDir::RIGHT;
    stepper.move(1000000L);
  }

  server.send(200, "application/json", statusJSON());
}

void handleJogStop() {
  stopMotor();
  server.send(200, "application/json", statusJSON());
}

void handleHome() {
  enableMotor();
  homing  = true;
  jogDir  = JogDir::NONE;

  stepper.setMaxSpeed(HOME_SPEED);
  stepper.setAcceleration(DEFAULT_ACCEL);
  stepper.move(-1000000L); // move left until limit switch

  server.send(200, "application/json", statusJSON());
}

void handleSpeed() {
  String uri = server.uri();
  String val = getParam(uri.substring(uri.indexOf('?') + 1), "val");

  if (val == "") { server.send(400, "text/plain", "Missing val"); return; }

  int spd = val.toInt();
  if (spd <= 0 || spd > 10000) { server.send(400, "text/plain", "Invalid speed"); return; }

  stepper.setMaxSpeed(spd);
  server.send(200, "application/json", statusJSON());
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ── Route registration helper (handles both /jog/start?dir=left style URIs) ──

void registerRoutes() {
  server.on("/status",     handleStatus);
  server.on("/move",       handleMove);
  server.on("/jog/stop",   handleJogStop);
  server.on("/jog/start",  handleJogStart);
  server.on("/home",       handleHome);
  server.on("/speed",      handleSpeed);
  server.onNotFound(handleNotFound);
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  // Pins
  pinMode(ENABLE_PIN,  OUTPUT);
  pinMode(LIMIT_LEFT,  INPUT_PULLUP);
  pinMode(LIMIT_RIGHT, INPUT_PULLUP);
  disableMotor(); // start with motor free-wheeling

  // Stepper defaults
  stepper.setMaxSpeed(DEFAULT_MAX_SPEED);
  stepper.setAcceleration(DEFAULT_ACCEL);

  // WiFi
#ifdef ESP32
  IPAddress ip, gw, subnet;
  ip.fromString(STATIC_IP);
  gw.fromString(GATEWAY_IP);
  subnet.fromString(SUBNET_MASK);
  WiFi.config(ip, gw, subnet);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println();
  Serial.print("IP: "); Serial.println(WiFi.localIP());
#else
  Serial1.begin(9600);
  WiFiEsp.init(&Serial1);
  WiFiEsp.begin(WIFI_SSID, WIFI_PASS, &server, HTTP_PORT);
  Serial.print("IP: "); Serial.println(WiFiEsp.localIP());
#endif

  registerRoutes();
  server.begin();
  Serial.println("HTTP server started");
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void loop() {
  server.handleClient();

  // ── Limit switch safety check ─────────────────────────────────────────────
  if (stepper.isRunning()) {
    if (jogDir == JogDir::LEFT || homing) {
      if (limitLeft()) {
        stepper.stop();
        if (homing) {
          stepper.setCurrentPosition(0);   // zero the position here
          stepper.setMaxSpeed(DEFAULT_MAX_SPEED);
          homing = false;
        }
        jogDir = JogDir::NONE;
        disableMotor();
      }
    }
    if (jogDir == JogDir::RIGHT) {
      if (limitRight()) {
        stepper.stop();
        jogDir = JogDir::NONE;
        disableMotor();
      }
    }
  }

  // Run one step (non-blocking)
  stepper.run();

  // Disable motor when idle to reduce heat
  if (!stepper.isRunning() && jogDir == JogDir::NONE && !homing) {
    disableMotor();
  }
}
