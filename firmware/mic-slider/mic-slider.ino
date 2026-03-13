// mic-slider.ino
// Arduino Uno R4 WiFi (ABX00087) + TB6600 driver + NEMA11 50mm linear slide
//
// Libraries (install via Arduino Library Manager):
//   - AccelStepper by Mike McCauley
//   - WiFiS3 is included with the Arduino UNO R4 board package (no install needed)
//
// TB6600 wiring:
//   PUL+ → pin 3 (STEP),  PUL- → GND
//   DIR+ → pin 4 (DIR),   DIR- → GND
//   ENA  → leave DISCONNECTED (motor always enabled, holds position when idle)
//   VMOT/GND → 24V supply
//
// HTTP endpoints (all GET):
//   GET /status                       → {"pos":0,"moving":false,"maxSteps":40000,...}
//   GET /move?dir=left&steps=200      → move N steps
//   GET /jog/start?dir=left           → begin continuous movement
//   GET /jog/stop                     → stop immediately
//   GET /home                         → run to left limit switch, zero position
//   GET /speed?val=1600               → set max speed (steps/sec)

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

// ── Limit switches ────────────────────────────────────────────────────────────

bool limitLeft()  { return digitalRead(LIMIT_LEFT)  == LOW; }
bool limitRight() { return digitalRead(LIMIT_RIGHT) == LOW; }

// ── Stop ──────────────────────────────────────────────────────────────────────

void stopMotor() {
  stepper.stop();
  unsigned long t = millis();
  while (stepper.isRunning() && millis() - t < 200) stepper.run();
  jogDir = JogDir::NONE;
  homing = false;
  // Motor stays energized (ENA disconnected) — holds position
}

// ── Query-string parser ───────────────────────────────────────────────────────

String getParam(const String& query, const String& key) {
  String search = key + "=";
  int start = query.indexOf(search);
  if (start == -1) return "";
  start += search.length();
  int end = query.indexOf('&', start);
  return (end == -1) ? query.substring(start) : query.substring(start, end);
}

// ── JSON status ───────────────────────────────────────────────────────────────

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

// ── HTTP helpers ──────────────────────────────────────────────────────────────

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
  sendJSON(client, code, "{\"error\":\"" + msg + "\"}");
}

// ── Route handlers ────────────────────────────────────────────────────────────

void handleStatus(WiFiClient& client, const String&) {
  sendJSON(client, 200, statusJSON());
}

void handleMove(WiFiClient& client, const String& query) {
  String dir   = getParam(query, "dir");
  String steps = getParam(query, "steps");
  if (dir.isEmpty() || steps.isEmpty()) { sendError(client, 400, "Missing dir or steps"); return; }
  long n = steps.toInt();
  if (n <= 0) { sendError(client, 400, "Invalid steps"); return; }

  homing = false;
  jogDir = JogDir::NONE;
  stepper.move(dir == "left" ? -n : n);
  sendJSON(client, 200, statusJSON());
}

void handleJogStart(WiFiClient& client, const String& query) {
  String dir = getParam(query, "dir");
  if (dir.isEmpty()) { sendError(client, 400, "Missing dir"); return; }

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
  homing = true;
  jogDir = JogDir::NONE;
  stepper.setMaxSpeed(HOME_SPEED);
  stepper.move(-1000000L); // move left until left limit switch fires
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

// ── HTTP dispatcher ───────────────────────────────────────────────────────────

void handleRequest(WiFiClient& client) {
  String requestLine = client.readStringUntil('\n');
  requestLine.trim();

  // Drain headers
  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.isEmpty()) break;
  }

  int firstSpace  = requestLine.indexOf(' ');
  int secondSpace = requestLine.lastIndexOf(' ');
  if (firstSpace == -1 || secondSpace == -1 || firstSpace == secondSpace) {
    sendError(client, 400, "Bad request"); return;
  }

  String method   = requestLine.substring(0, firstSpace);
  String fullPath = requestLine.substring(firstSpace + 1, secondSpace);

  if (method != "GET") { sendError(client, 405, "Method not allowed"); return; }

  int qmark   = fullPath.indexOf('?');
  String path  = (qmark == -1) ? fullPath : fullPath.substring(0, qmark);
  String query = (qmark == -1) ? ""       : fullPath.substring(qmark + 1);

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
  while (!Serial && millis() < 3000);

  pinMode(LIMIT_LEFT,  INPUT_PULLUP);
  pinMode(LIMIT_RIGHT, INPUT_PULLUP);

  stepper.setMaxSpeed(DEFAULT_MAX_SPEED);
  stepper.setAcceleration(DEFAULT_ACCEL);

  // Using DHCP — WiFi.config() has known reliability issues on WiFiS3.
  // Read the actual IP from Serial Monitor after connecting, then set it
  // as your router's DHCP reservation for a stable address.
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  // Wait for DHCP to assign an IP
  while (WiFi.localIP() == IPAddress(0, 0, 0, 0)) { delay(500); Serial.print("d"); }
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.begin();
  Serial.println("HTTP server started");
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void loop() {
  // ── HTTP ──────────────────────────────────────────────────────────────────
  WiFiClient client = server.available();
  if (client) {
    unsigned long t = millis();
    while (!client.available() && millis() - t < 1000);
    if (client.available()) handleRequest(client);
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
    }
    if (jogDir == JogDir::RIGHT && limitRight()) {
      stepper.stop();
      jogDir = JogDir::NONE;
    }
  }

  stepper.run();
}
