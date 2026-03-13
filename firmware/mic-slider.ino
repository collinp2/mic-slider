// mic-slider.ino
// Arduino Uno R4 WiFi (ABX00087) — WiFi-controlled stepper motor slider.
//
// Libraries (install via Arduino Library Manager):
//   - AccelStepper by Mike McCauley
//   - WiFiS3 is included with the Arduino UNO R4 board package (no install needed)
//
// HTTP endpoints (all GET, plain-text/JSON responses):
//   GET /status                       → {"pos":0,"moving":false,"maxSteps":20000,...}
//   GET /move?dir=left&steps=200      → move N steps
//   GET /jog/start?dir=left           → begin continuous movement
//   GET /jog/stop                     → stop immediately
//   GET /home                         → run to left limit switch, zero position
//   GET /speed?val=800                → set max speed (steps/sec)

#include <WiFiS3.h>
#include <AccelStepper.h>
#include "config.h"

// ── Server ────────────────────────────────────────────────────────────────────
WiFiServer server(HTTP_PORT);

// ── Stepper ───────────────────────────────────────────────────────────────────
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// ── State ─────────────────────────────────────────────────────────────────────
enum class JogDir { NONE, LEFT, RIGHT };
JogDir jogDir = JogDir::NONE;
bool   homing = false;
int    currentMaxSpeed = DEFAULT_MAX_SPEED;

// ── Motor helpers ─────────────────────────────────────────────────────────────

bool limitLeft()  { return digitalRead(LIMIT_LEFT)  == LOW; }
bool limitRight() { return digitalRead(LIMIT_RIGHT) == LOW; }

void enableMotor()  { digitalWrite(ENABLE_PIN, LOW); }
void disableMotor() { digitalWrite(ENABLE_PIN, HIGH); }

void stopMotor() {
  stepper.stop();
  // Run a few cycles to let AccelStepper decelerate to 0
  unsigned long t = millis();
  while (stepper.isRunning() && millis() - t < 200) stepper.run();
  jogDir = JogDir::NONE;
  homing = false;
  disableMotor();
}

// ── Query-string parser ───────────────────────────────────────────────────────

// Extract the value of `key` from "dir=left&steps=200" style strings.
String getParam(const String& query, const String& key) {
  String search = key + "=";
  int start = query.indexOf(search);
  if (start == -1) return "";
  start += search.length();
  int end = query.indexOf('&', start);
  return (end == -1) ? query.substring(start) : query.substring(start, end);
}

// ── JSON status builder ───────────────────────────────────────────────────────

String statusJSON() {
  String j = "{\"pos\":";
  j += stepper.currentPosition();
  j += ",\"moving\":";
  j += stepper.isRunning() ? "true" : "false";
  j += ",\"maxSteps\":";
  j += MAX_STEPS;
  j += ",\"jogDir\":\"";
  j += (jogDir == JogDir::LEFT  ? "left"  :
        jogDir == JogDir::RIGHT ? "right" : "none");
  j += "\",\"homing\":";
  j += homing ? "true" : "false";
  j += ",\"speed\":";
  j += currentMaxSpeed;
  j += "}";
  return j;
}

// ── HTTP response senders ─────────────────────────────────────────────────────

void sendJSON(WiFiClient& client, int code, const String& body) {
  client.print("HTTP/1.1 ");
  client.print(code);
  client.println(code == 200 ? " OK" : " Error");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.print("Content-Length: ");
  client.println(body.length());
  client.println();
  client.print(body);
}

void sendError(WiFiClient& client, int code, const String& msg) {
  String body = "{\"error\":\"" + msg + "\"}";
  sendJSON(client, code, body);
}

// ── Route handlers ────────────────────────────────────────────────────────────

void handleStatus(WiFiClient& client, const String&) {
  sendJSON(client, 200, statusJSON());
}

void handleMove(WiFiClient& client, const String& query) {
  String dir   = getParam(query, "dir");
  String steps = getParam(query, "steps");
  if (dir.isEmpty() || steps.isEmpty()) {
    sendError(client, 400, "Missing dir or steps"); return;
  }
  long n = steps.toInt();
  if (n <= 0) { sendError(client, 400, "Invalid steps"); return; }

  enableMotor();
  homing = false;
  jogDir = JogDir::NONE;
  stepper.move(dir == "left" ? -n : n);
  sendJSON(client, 200, statusJSON());
}

void handleJogStart(WiFiClient& client, const String& query) {
  String dir = getParam(query, "dir");
  if (dir.isEmpty()) { sendError(client, 400, "Missing dir"); return; }

  enableMotor();
  homing = false;
  if (dir == "left") {
    jogDir = JogDir::LEFT;
    stepper.move(-1000000L);
  } else {
    jogDir = JogDir::RIGHT;
    stepper.move(1000000L);
  }
  sendJSON(client, 200, statusJSON());
}

void handleJogStop(WiFiClient& client, const String&) {
  stopMotor();
  sendJSON(client, 200, statusJSON());
}

void handleHome(WiFiClient& client, const String&) {
  enableMotor();
  homing  = true;
  jogDir  = JogDir::NONE;
  stepper.setMaxSpeed(HOME_SPEED);
  stepper.move(-1000000L); // move left until limit switch fires
  sendJSON(client, 200, statusJSON());
}

void handleSpeed(WiFiClient& client, const String& query) {
  String val = getParam(query, "val");
  if (val.isEmpty()) { sendError(client, 400, "Missing val"); return; }
  int spd = val.toInt();
  if (spd < 1 || spd > 10000) { sendError(client, 400, "Speed out of range"); return; }
  currentMaxSpeed = spd;
  stepper.setMaxSpeed(spd);
  sendJSON(client, 200, statusJSON());
}

// ── HTTP request dispatcher ───────────────────────────────────────────────────

void handleRequest(WiFiClient& client) {
  // Read the request line (e.g. "GET /jog/start?dir=left HTTP/1.1")
  String requestLine = client.readStringUntil('\n');
  requestLine.trim();

  // Drain remaining headers
  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.isEmpty()) break;
  }

  // Parse method and path+query
  int firstSpace  = requestLine.indexOf(' ');
  int secondSpace = requestLine.lastIndexOf(' ');
  if (firstSpace == -1 || secondSpace == -1 || firstSpace == secondSpace) {
    sendError(client, 400, "Bad request"); return;
  }

  String method  = requestLine.substring(0, firstSpace);
  String fullPath = requestLine.substring(firstSpace + 1, secondSpace);

  if (method != "GET") { sendError(client, 405, "Method not allowed"); return; }

  // Split path and query string
  int qmark = fullPath.indexOf('?');
  String path  = (qmark == -1) ? fullPath : fullPath.substring(0, qmark);
  String query = (qmark == -1) ? ""       : fullPath.substring(qmark + 1);

  // Route
  if      (path == "/status")    handleStatus(client, query);
  else if (path == "/move")      handleMove(client, query);
  else if (path == "/jog/start") handleJogStart(client, query);
  else if (path == "/jog/stop")  handleJogStop(client, query);
  else if (path == "/home")      handleHome(client, query);
  else if (path == "/speed")     handleSpeed(client, query);
  else                           sendError(client, 404, "Not found");
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);  // wait up to 3s for Serial Monitor

  // Pins
  pinMode(ENABLE_PIN,  OUTPUT);
  pinMode(LIMIT_LEFT,  INPUT_PULLUP);
  pinMode(LIMIT_RIGHT, INPUT_PULLUP);
  disableMotor();

  // Stepper defaults
  stepper.setMaxSpeed(DEFAULT_MAX_SPEED);
  stepper.setAcceleration(DEFAULT_ACCEL);

  // WiFi — set static IP before connecting
  WiFi.config(STATIC_IP, GATEWAY_IP, SUBNET_MASK);
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  int status = WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.begin();
  Serial.println("HTTP server started");
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void loop() {
  // ── Handle incoming HTTP client ───────────────────────────────────────────
  WiFiClient client = server.available();
  if (client) {
    unsigned long timeout = millis();
    while (!client.available() && millis() - timeout < 1000); // wait for data
    if (client.available()) {
      handleRequest(client);
    }
    client.stop();
  }

  // ── Limit switch safety ───────────────────────────────────────────────────
  if (stepper.isRunning()) {
    if ((jogDir == JogDir::LEFT || homing) && limitLeft()) {
      stepper.stop();
      if (homing) {
        stepper.setCurrentPosition(0);
        stepper.setMaxSpeed(currentMaxSpeed);
        homing = false;
      }
      jogDir = JogDir::NONE;
      disableMotor();
    }
    if (jogDir == JogDir::RIGHT && limitRight()) {
      stepper.stop();
      jogDir = JogDir::NONE;
      disableMotor();
    }
  }

  // ── Step the motor ────────────────────────────────────────────────────────
  stepper.run();

  // ── Disable when idle ─────────────────────────────────────────────────────
  if (!stepper.isRunning() && jogDir == JogDir::NONE && !homing) {
    disableMotor();
  }
}
