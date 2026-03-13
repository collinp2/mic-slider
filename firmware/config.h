// config.h — WiFi and pin configuration for mic-slider firmware
// Arduino Uno R4 WiFi (ABX00087) + TB6600 driver + NEMA11 linear slide
// Edit these values before flashing.

#ifndef CONFIG_H
#define CONFIG_H

// ── WiFi ──────────────────────────────────────────────────────────────────────
#define WIFI_SSID     "YourSSID"
#define WIFI_PASS     "YourPassword"

// Static IP (call WiFi.config() BEFORE WiFi.begin() for this to take effect)
#define STATIC_IP     IPAddress(192, 168, 1, 42)
#define GATEWAY_IP    IPAddress(192, 168, 1,  1)
#define SUBNET_MASK   IPAddress(255, 255, 255, 0)

// HTTP server port
#define HTTP_PORT     80

// ── Stepper motor pins (TB6600 driver) ────────────────────────────────────────
// TB6600 wiring (single-ended mode):
//   PUL+ → STEP_PIN,  PUL- → GND
//   DIR+ → DIR_PIN,   DIR- → GND
//   ENA  → leave DISCONNECTED (motor always enabled = holds position)
//
// Power: 24V supply to TB6600 VMOT/GND.
// Arduino powered separately via USB or barrel jack.
#define STEP_PIN      3
#define DIR_PIN       4
// No ENABLE_PIN — ENA left disconnected on TB6600

// ── Limit switches ────────────────────────────────────────────────────────────
// Wire each switch between the pin and GND (INPUT_PULLUP: LOW = triggered)
#define LIMIT_LEFT    6
#define LIMIT_RIGHT   7

// ── Motion parameters ─────────────────────────────────────────────────────────
// Microstepping is set via DIP switches on the TB6600, not in firmware.
// Recommended: 1/8 step (SW1=ON SW2=ON SW3=OFF on most TB6600 units).
//
// MAX_STEPS calibration for 50mm NEMA11 linear slide:
//   Lead screw pitch is typically 2mm/rev for these slides.
//   200 full steps/rev × 4 (1/8 microstep / 2mm pitch) = 800 steps/mm
//   Wait — at 1/8 step: 1600 steps/rev ÷ 2mm = 800 steps/mm × 50mm = 40,000 steps
//
//   If your lead screw pitch is different, adjust accordingly:
//     Full step:  200 steps/rev
//     1/2 step:   400 steps/rev  → 50mm at 2mm pitch = 10,000 steps
//     1/4 step:   800 steps/rev  → 50mm at 2mm pitch = 20,000 steps
//     1/8 step:  1600 steps/rev  → 50mm at 2mm pitch = 40,000 steps (recommended)
//
// Set this after you measure actual travel distance per step on your hardware.
#define DEFAULT_MAX_SPEED   1600    // steps/sec
#define DEFAULT_ACCEL       800     // steps/sec²
#define HOME_SPEED          400     // steps/sec during homing (slow and safe)
#define MAX_STEPS           40000   // ~50mm at 1/8 step, 2mm pitch — calibrate!

#endif // CONFIG_H
