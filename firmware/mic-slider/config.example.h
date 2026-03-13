// config.example.h — copy this to config.h and fill in your values.
// config.h is gitignored and will never be committed.

#ifndef CONFIG_H
#define CONFIG_H

// ── WiFi ──────────────────────────────────────────────────────────────────────
#define WIFI_SSID     "YourSSID"
#define WIFI_PASS     "YourPassword"

// Static IP (call WiFi.config() BEFORE WiFi.begin() for this to take effect)
#define STATIC_IP     IPAddress(10, 0, 0, 42)
#define GATEWAY_IP    IPAddress(10, 0, 0,  1)
#define SUBNET_MASK   IPAddress(255, 255, 255, 0)

// HTTP server port
#define HTTP_PORT     8080

// ── Stepper motor pins (2× TB6600 drivers) ───────────────────────────────────
// TB6600 wiring (single-ended mode):
//   PUL+ → STEP_PIN,  PUL- → GND
//   DIR+ → DIR_PIN,   DIR- → GND
//   ENA  → leave DISCONNECTED (motor always enabled = holds position)
//
// Power: 24V supply to TB6600 VMOT/GND.
// Arduino powered separately via USB or barrel jack.
#define STEP_PIN_1    3
#define DIR_PIN_1     4
#define STEP_PIN_2    8
#define DIR_PIN_2     9

// ── Limit switches ────────────────────────────────────────────────────────────
// Wire each switch between the pin and GND (INPUT_PULLUP: LOW = triggered)
#define LIMIT_LEFT_1   6
#define LIMIT_RIGHT_1  7
#define LIMIT_LEFT_2   10
#define LIMIT_RIGHT_2  11

// ── Motion parameters ─────────────────────────────────────────────────────────
// Microstepping is set via DIP switches on the TB6600, not in firmware.
// Recommended: 1/8 step (SW1=ON SW2=ON SW3=OFF on most TB6600 units).
//
// MAX_STEPS calibration for 50mm NEMA11 linear slide:
//   Lead screw pitch is typically 2mm/rev for these slides.
//   At 1/8 step: 1600 steps/rev ÷ 2mm pitch = 800 steps/mm × 50mm = 40,000 steps
//
// Calibrate by homing, jogging to full travel, reading position from /status.
#define DEFAULT_MAX_SPEED   20000   // steps/sec (~25mm/sec at 1/8 step)
#define DEFAULT_ACCEL       16000   // steps/sec² — high accel lets motor reach top speed in 50mm
#define HOME_SPEED          800     // steps/sec during homing (slow and safe)
#define MAX_STEPS           40000   // ~50mm at 1/8 step, 2mm pitch — calibrate!

#endif // CONFIG_H
