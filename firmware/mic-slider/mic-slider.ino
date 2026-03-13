// mic-slider.ino
// Arduino Uno R4 WiFi (ABX00087) + 2× TB6600 driver + 2× NEMA11 50mm linear slide
//
// Libraries (install via Arduino Library Manager):
//   - AccelStepper by Mike McCauley
//   - WiFiS3 is included with the Arduino UNO R4 board package (no install needed)
//
// HTTP endpoints (all GET) — add ?slider=1 or ?slider=2 (default: 1):
//   GET /status?slider=1                      → {"pos":0,"moving":false,...}
//   GET /move?slider=1&dir=left&steps=200     → move N steps
//   GET /jog/start?slider=1&dir=left          → begin continuous movement
//   GET /jog/stop?slider=1                    → stop immediately
//   GET /home?slider=1                        → run to left limit switch, zero position
//   GET /speed?slider=1&val=8000              → set max speed (steps/sec)

#include <WiFiS3.h>
#include <AccelStepper.h>
#include "config.h"

// ── Server ────────────────────────────────────────────────────────────────────
WiFiServer server(HTTP_PORT);

// ── Steppers ──────────────────────────────────────────────────────────────────
AccelStepper stepper0(AccelStepper::DRIVER, STEP_PIN_1, DIR_PIN_1);
AccelStepper stepper1(AccelStepper::DRIVER, STEP_PIN_2, DIR_PIN_2);
AccelStepper* const steppers[2] = { &stepper0, &stepper1 };

// ── State (per slider) ────────────────────────────────────────────────────────
enum class JogDir { NONE, LEFT, RIGHT };
JogDir jogDir[2]       = { JogDir::NONE, JogDir::NONE };
bool   homing[2]       = { false, false };
int    currentMaxSpeed[2] = { DEFAULT_MAX_SPEED, DEFAULT_MAX_SPEED };

// ── Limit switches ────────────────────────────────────────────────────────────

bool limitLeft(int i)  { return digitalRead(i == 0 ? LIMIT_LEFT_1  : LIMIT_LEFT_2)  == LOW; }
bool limitRight(int i) { return digitalRead(i == 0 ? LIMIT_RIGHT_1 : LIMIT_RIGHT_2) == LOW; }

// ── Stop ──────────────────────────────────────────────────────────────────────

void stopMotor(int i) {
  steppers[i]->stop();
  unsigned long t = millis();
  while (steppers[i]->isRunning() && millis() - t < 200) steppers[i]->run();
  jogDir[i] = JogDir::NONE;
  homing[i] = false;
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

// Returns 0-based slider index from ?slider=1 or ?slider=2 (default: 0)
int sliderIdx(const String& query) {
  String s = getParam(query, "slider");
  return (s == "2") ? 1 : 0;
}

// ── JSON status ───────────────────────────────────────────────────────────────

String statusJSON(int i) {
  String j = "{\"pos\":";
  j += steppers[i]->currentPosition();
  j += ",\"moving\":";
  j += steppers[i]->isRunning() ? "true" : "false";
  j += ",\"maxSteps\":";
  j += MAX_STEPS;
  j += ",\"slider\":";
  j += (i + 1);
  j += ",\"jogDir\":\"";
  j += (jogDir[i] == JogDir::LEFT  ? "left"  :
        jogDir[i] == JogDir::RIGHT ? "right" : "none");
  j += "\",\"homing\":";
  j += homing[i] ? "true" : "false";
  j += ",\"speed\":";
  j += currentMaxSpeed[i];
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

void handleStatus(WiFiClient& client, const String& query) {
  sendJSON(client, 200, statusJSON(sliderIdx(query)));
}

void handleMove(WiFiClient& client, const String& query) {
  int i = sliderIdx(query);
  String dir   = getParam(query, "dir");
  String steps = getParam(query, "steps");
  if (dir.isEmpty() || steps.isEmpty()) { sendError(client, 400, "Missing dir or steps"); return; }
  long n = steps.toInt();
  if (n <= 0) { sendError(client, 400, "Invalid steps"); return; }

  homing[i] = false;
  jogDir[i]  = JogDir::NONE;
  steppers[i]->move(dir == "left" ? -n : n);
  sendJSON(client, 200, statusJSON(i));
}

void handleJogStart(WiFiClient& client, const String& query) {
  int i = sliderIdx(query);
  String dir = getParam(query, "dir");
  if (dir.isEmpty()) { sendError(client, 400, "Missing dir"); return; }

  homing[i] = false;
  if (dir == "left") {
    jogDir[i] = JogDir::LEFT;
    steppers[i]->move(-1000000L);
  } else {
    jogDir[i] = JogDir::RIGHT;
    steppers[i]->move(1000000L);
  }
  sendJSON(client, 200, statusJSON(i));
}

void handleJogStop(WiFiClient& client, const String& query) {
  int i = sliderIdx(query);
  stopMotor(i);
  sendJSON(client, 200, statusJSON(i));
}

void handleHome(WiFiClient& client, const String& query) {
  int i = sliderIdx(query);
  homing[i] = true;
  jogDir[i]  = JogDir::NONE;
  steppers[i]->setMaxSpeed(HOME_SPEED);
  steppers[i]->move(-1000000L);
  sendJSON(client, 200, statusJSON(i));
}

void handleSpeed(WiFiClient& client, const String& query) {
  int i = sliderIdx(query);
  String val = getParam(query, "val");
  if (val.isEmpty()) { sendError(client, 400, "Missing val"); return; }
  int spd = val.toInt();
  if (spd < 1 || spd > 20000) { sendError(client, 400, "Speed out of range"); return; }
  currentMaxSpeed[i] = spd;
  steppers[i]->setMaxSpeed(spd);
  sendJSON(client, 200, statusJSON(i));
}

// ── HTTP dispatcher ───────────────────────────────────────────────────────────

void handleRequest(WiFiClient& client) {
  String requestLine = "";
  String currentLine = "";
  bool firstLine = true;
  unsigned long t = millis();

  while (client.connected() && millis() - t < 3000) {
    stepper0.run();
    stepper1.run();
    if (client.available()) {
      char c = client.read();
      if (c == '\n') {
        if (firstLine) { requestLine = currentLine; firstLine = false; }
        if (currentLine.length() == 0 || currentLine == "\r") break;
        currentLine = "";
      } else if (c != '\r') {
        currentLine += c;
      }
    }
  }

  requestLine.trim();
  int firstSpace  = requestLine.indexOf(' ');
  int secondSpace = requestLine.lastIndexOf(' ');
  if (firstSpace == -1 || secondSpace == -1 || firstSpace == secondSpace) {
    sendError(client, 400, "Bad request"); return;
  }

  String method   = requestLine.substring(0, firstSpace);
  String fullPath = requestLine.substring(firstSpace + 1, secondSpace);

  if (method != "GET") { sendError(client, 405, "Method not allowed"); return; }

  int qmark    = fullPath.indexOf('?');
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

  pinMode(LIMIT_LEFT_1,  INPUT_PULLUP);
  pinMode(LIMIT_RIGHT_1, INPUT_PULLUP);
  pinMode(LIMIT_LEFT_2,  INPUT_PULLUP);
  pinMode(LIMIT_RIGHT_2, INPUT_PULLUP);

  for (int i = 0; i < 2; i++) {
    steppers[i]->setMaxSpeed(DEFAULT_MAX_SPEED);
    steppers[i]->setAcceleration(DEFAULT_ACCEL);
  }

  WiFi.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
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
  static unsigned long lastWiFiCheck = 0;
  unsigned long now = millis();
  if (now - lastWiFiCheck >= 50) {
    lastWiFiCheck = now;
    WiFiClient client = server.available();
    if (client) {
      handleRequest(client);
      client.stop();
    }
  }

  // ── Limit switch safety (both sliders) ────────────────────────────────────
  for (int i = 0; i < 2; i++) {
    if (steppers[i]->isRunning()) {
      if ((jogDir[i] == JogDir::LEFT || homing[i]) && limitLeft(i)) {
        steppers[i]->stop();
        if (homing[i]) {
          steppers[i]->setCurrentPosition(0);
          steppers[i]->setMaxSpeed(currentMaxSpeed[i]);
          homing[i] = false;
        }
        jogDir[i] = JogDir::NONE;
      }
      if (jogDir[i] == JogDir::RIGHT && limitRight(i)) {
        steppers[i]->stop();
        jogDir[i] = JogDir::NONE;
      }
    }
  }

  stepper0.run();
  stepper1.run();
}
