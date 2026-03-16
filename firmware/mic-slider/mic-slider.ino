// mic-slider.ino — WiFi mic slider controller
// Arduino Uno R4 WiFi + 3× Adafruit Motor Shield v2 (stacked, 0x60/0x61/0x62)
// 6× NEMA11 T6x1 linear slide: 1mm pitch, 200 steps/mm, 100mm travel
//
// Libraries (install via Arduino Library Manager):
//   - Adafruit Motor Shield V2 Library (by Adafruit)
//   - AccelStepper (by Mike McCauley)
//
// HTTP endpoints (all GET, JSON, ?slider=1..6):
//   GET /status      — current state
//   GET /move        — move N steps left or right
//   GET /jog/start   — continuous movement until /jog/stop or limit hit
//   GET /jog/stop    — stop with deceleration
//   GET /home        — run left to limit switch, zero position
//   GET /speed       — set max speed (steps/sec, 100–3000)

#include <WiFiS3.h>
#include <AccelStepper.h>
#include <Adafruit_MotorShield.h>
#include <Wire.h>
#include <EEPROM.h>
#include "config.h"

// ── Motor Shields (3 stacked) ─────────────────────────────────────────────────
Adafruit_MotorShield shields[3] = {
    Adafruit_MotorShield(0x60),
    Adafruit_MotorShield(0x61),
    Adafruit_MotorShield(0x62)
};
Adafruit_StepperMotor *mtr[NUM_SLIDERS];

// ── Step callbacks ────────────────────────────────────────────────────────────
// AccelStepper requires plain function pointers — one forward/backward pair per motor.
// FORWARD = positive position (right, away from home)
// BACKWARD = negative position (left, toward home limit switch)
void s0f(){mtr[0]->onestep(FORWARD, DOUBLE);}  void s0b(){mtr[0]->onestep(BACKWARD,DOUBLE);}
void s1f(){mtr[1]->onestep(FORWARD, DOUBLE);}  void s1b(){mtr[1]->onestep(BACKWARD,DOUBLE);}
void s2f(){mtr[2]->onestep(FORWARD, DOUBLE);}  void s2b(){mtr[2]->onestep(BACKWARD,DOUBLE);}
void s3f(){mtr[3]->onestep(FORWARD, DOUBLE);}  void s3b(){mtr[3]->onestep(BACKWARD,DOUBLE);}
void s4f(){mtr[4]->onestep(FORWARD, DOUBLE);}  void s4b(){mtr[4]->onestep(BACKWARD,DOUBLE);}
void s5f(){mtr[5]->onestep(FORWARD, DOUBLE);}  void s5b(){mtr[5]->onestep(BACKWARD,DOUBLE);}

// ── AccelStepper instances ────────────────────────────────────────────────────
AccelStepper steppers[NUM_SLIDERS] = {
    AccelStepper(s0f, s0b),
    AccelStepper(s1f, s1b),
    AccelStepper(s2f, s2b),
    AccelStepper(s3f, s3b),
    AccelStepper(s4f, s4b),
    AccelStepper(s5f, s5b)
};

// ── Per-slider state ──────────────────────────────────────────────────────────
enum class JogDir { NONE, LEFT, RIGHT };
JogDir jogDir[NUM_SLIDERS]    = {};
bool   homing[NUM_SLIDERS]    = {};
float  maxSpeed[NUM_SLIDERS];
bool   wasRunning[NUM_SLIDERS] = {};

// ── WiFi & HTTP server ────────────────────────────────────────────────────────
WiFiServer server(HTTP_PORT);

// ─────────────────────────────────────────────────────────────────────────────
// EEPROM helpers
// ─────────────────────────────────────────────────────────────────────────────

void savePosition(int i) {
    long pos = steppers[i].currentPosition();
    EEPROM.put(EEPROM_POS_ADDR + i * 4, pos);
}

void loadPositions() {
    byte magic;
    EEPROM.get(EEPROM_MAGIC_ADDR, magic);
    if (magic != (byte)EEPROM_MAGIC_VAL) {
        // First boot — initialize all positions to 0
        for (int i = 0; i < NUM_SLIDERS; i++) {
            steppers[i].setCurrentPosition(0);
            savePosition(i);
        }
        EEPROM.put(EEPROM_MAGIC_ADDR, (byte)EEPROM_MAGIC_VAL);
        return;
    }
    for (int i = 0; i < NUM_SLIDERS; i++) {
        long pos;
        EEPROM.get(EEPROM_POS_ADDR + i * 4, pos);
        pos = constrain(pos, 0L, (long)MAX_STEPS);
        steppers[i].setCurrentPosition(pos);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Limit switches
// ─────────────────────────────────────────────────────────────────────────────
bool limitLeft(int i)  { return digitalRead(LIMIT_LEFT_PINS[i])  == LOW; }
bool limitRight(int i) { return digitalRead(LIMIT_RIGHT_PINS[i]) == LOW; }

// ─────────────────────────────────────────────────────────────────────────────
// HTTP helpers
// ─────────────────────────────────────────────────────────────────────────────

int queryInt(const String &query, const char *key) {
    String k = String("?") + key + "=";
    int idx = query.indexOf(k);
    if (idx < 0) { k = String("&") + key + "="; idx = query.indexOf(k); }
    if (idx < 0) return -1;
    return query.substring(idx + k.length()).toInt();
}

String queryStr(const String &query, const char *key) {
    String k = String("?") + key + "=";
    int idx = query.indexOf(k);
    if (idx < 0) { k = String("&") + key + "="; idx = query.indexOf(k); }
    if (idx < 0) return "";
    String val = query.substring(idx + k.length());
    int amp = val.indexOf('&');
    if (amp >= 0) val = val.substring(0, amp);
    return val;
}

// API uses 1-indexed sliders; convert to 0-indexed internally
int sliderIdx(const String &query) {
    int n = queryInt(query, "slider");
    if (n < 1 || n > NUM_SLIDERS) return 0;
    return n - 1;
}

void sendOK(WiFiClient &client, const String &body) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println(body);
}

String statusJSON(int i) {
    String dir = "none";
    if (jogDir[i] == JogDir::LEFT)  dir = "left";
    if (jogDir[i] == JogDir::RIGHT) dir = "right";
    return String("{\"slider\":") + (i + 1) +
           ",\"pos\":"      + steppers[i].currentPosition() +
           ",\"moving\":"   + (steppers[i].isRunning() ? "true" : "false") +
           ",\"maxSteps\":" + MAX_STEPS +
           ",\"jogDir\":\""  + dir + "\"" +
           ",\"homing\":"   + (homing[i] ? "true" : "false") +
           ",\"speed\":"    + (int)maxSpeed[i] +
           "}";
}

// ─────────────────────────────────────────────────────────────────────────────
// Request handler
// ─────────────────────────────────────────────────────────────────────────────
void handleRequest(WiFiClient &client) {
    String request = "";
    unsigned long t = millis();
    while (client.connected() && millis() - t < 1000) {
        if (client.available()) {
            char c = client.read();
            request += c;
            if (request.endsWith("\r\n\r\n")) break;
            // Keep steppers running while reading to prevent starvation
            for (int i = 0; i < NUM_SLIDERS; i++) steppers[i].run();
        }
    }

    int sp1 = request.indexOf(' ');
    int sp2 = request.indexOf(' ', sp1 + 1);
    if (sp1 < 0 || sp2 < 0) return;
    String pathQ = request.substring(sp1 + 1, sp2);
    int qIdx     = pathQ.indexOf('?');
    String path  = (qIdx >= 0) ? pathQ.substring(0, qIdx) : pathQ;
    String query = (qIdx >= 0) ? pathQ.substring(qIdx)    : "";

    int i = sliderIdx(query);

    if (path == "/status") {
        sendOK(client, statusJSON(i));

    } else if (path == "/move") {
        String dir = queryStr(query, "dir");
        int steps  = queryInt(query, "steps");
        if (steps <= 0) steps = 200;
        long target = steppers[i].currentPosition() +
                      (dir == "left" ? -(long)steps : (long)steps);
        target = constrain(target, 0L, (long)MAX_STEPS);
        steppers[i].setMaxSpeed(maxSpeed[i]);
        steppers[i].moveTo(target);
        jogDir[i] = JogDir::NONE;
        homing[i] = false;
        sendOK(client, statusJSON(i));

    } else if (path == "/jog/start") {
        String dir = queryStr(query, "dir");
        steppers[i].setMaxSpeed(maxSpeed[i]);
        if (dir == "left") {
            steppers[i].moveTo(-(long)MAX_STEPS * 2);
            jogDir[i] = JogDir::LEFT;
        } else {
            steppers[i].moveTo((long)MAX_STEPS * 2);
            jogDir[i] = JogDir::RIGHT;
        }
        homing[i] = false;
        sendOK(client, statusJSON(i));

    } else if (path == "/jog/stop") {
        steppers[i].stop();
        jogDir[i] = JogDir::NONE;
        homing[i] = false;
        sendOK(client, statusJSON(i));

    } else if (path == "/home") {
        steppers[i].setMaxSpeed(HOME_SPEED);
        steppers[i].moveTo(-(long)MAX_STEPS * 2);
        jogDir[i] = JogDir::LEFT;
        homing[i] = true;
        sendOK(client, statusJSON(i));

    } else if (path == "/speed") {
        int val = queryInt(query, "val");
        if (val > 0) {
            maxSpeed[i] = constrain((float)val, 100.0f, 3000.0f);
            if (!homing[i]) steppers[i].setMaxSpeed(maxSpeed[i]);
        }
        sendOK(client, statusJSON(i));

    } else {
        client.println("HTTP/1.1 404 Not Found");
        client.println("Connection: close");
        client.println();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // Limit switches
    for (int i = 0; i < NUM_SLIDERS; i++) {
        pinMode(LIMIT_LEFT_PINS[i],  INPUT_PULLUP);
        pinMode(LIMIT_RIGHT_PINS[i], INPUT_PULLUP);
    }

    // Motor Shields
    for (int s = 0; s < 3; s++) {
        shields[s].begin();
    }

    // Assign motors: shield 0 → sliders 0,1 | shield 1 → sliders 2,3 | shield 2 → sliders 4,5
    mtr[0] = shields[0].getStepper(200, 1);
    mtr[1] = shields[0].getStepper(200, 2);
    mtr[2] = shields[1].getStepper(200, 1);
    mtr[3] = shields[1].getStepper(200, 2);
    mtr[4] = shields[2].getStepper(200, 1);
    mtr[5] = shields[2].getStepper(200, 2);

    // AccelStepper config
    for (int i = 0; i < NUM_SLIDERS; i++) {
        maxSpeed[i] = DEFAULT_MAX_SPEED;
        steppers[i].setMaxSpeed(maxSpeed[i]);
        steppers[i].setAcceleration(DEFAULT_ACCEL);
    }

    // Restore positions from EEPROM
    loadPositions();

    // WiFi
    WiFi.config(STATIC_IP, GATEWAY_IP, SUBNET_MASK);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    server.begin();
    Serial.print("IP: "); Serial.println(WiFi.localIP());
}

// ─────────────────────────────────────────────────────────────────────────────
// Main loop
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    // Throttle WiFi polling to every 50ms — prevents TCP overhead from
    // starving stepper callbacks (each step = one I2C transaction via Motor Shield)
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

    // Run all steppers + limit switch checks
    for (int i = 0; i < NUM_SLIDERS; i++) {
        steppers[i].run();

        if (steppers[i].isRunning()) {
            // Left limit: stop, zero position, restore speed if homing
            if ((jogDir[i] == JogDir::LEFT || homing[i]) && limitLeft(i)) {
                steppers[i].stop();
                steppers[i].setCurrentPosition(0);
                if (homing[i]) {
                    homing[i] = false;
                    steppers[i].setMaxSpeed(maxSpeed[i]);
                }
                jogDir[i] = JogDir::NONE;
            }
            // Right limit: stop
            if (jogDir[i] == JogDir::RIGHT && limitRight(i)) {
                steppers[i].stop();
                jogDir[i] = JogDir::NONE;
            }
        }

        // Detect running → stopped transition: save position, release coils
        // T6x1 lead screw is self-locking — slider will not drift without power
        bool running = steppers[i].isRunning();
        if (wasRunning[i] && !running) {
            if (jogDir[i]  != JogDir::NONE) jogDir[i] = JogDir::NONE;
            if (homing[i])  { homing[i] = false; steppers[i].setMaxSpeed(maxSpeed[i]); }
            savePosition(i);
            mtr[i]->release();
        }
        wasRunning[i] = running;
    }
}
