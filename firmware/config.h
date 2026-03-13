// config.h — WiFi and pin configuration for mic-slider firmware
// Edit these values before flashing.

#ifndef CONFIG_H
#define CONFIG_H

// ── WiFi ──────────────────────────────────────────────────────────────────────
#define WIFI_SSID     "YourSSID"
#define WIFI_PASS     "YourPassword"

// Static IP configuration (recommended — avoids DHCP lookup delays)
// Set STATIC_IP to the address you want this slider to use.
// Leave GATEWAY and SUBNET matching your router.
#define STATIC_IP     "192.168.1.42"
#define GATEWAY_IP    "192.168.1.1"
#define SUBNET_MASK   "255.255.255.0"

// HTTP server port
#define HTTP_PORT     80

// ── Stepper motor pins (A4988 driver) ─────────────────────────────────────────
#define STEP_PIN      3
#define DIR_PIN       4
#define ENABLE_PIN    5   // LOW = enabled, HIGH = disabled (free-wheels motor)

// ── Limit switches (wired NC to GND; INPUT_PULLUP so HIGH = open) ─────────────
#define LIMIT_LEFT    6
#define LIMIT_RIGHT   7

// ── Motion parameters ─────────────────────────────────────────────────────────
#define DEFAULT_MAX_SPEED   800     // steps/sec
#define DEFAULT_ACCEL       400     // steps/sec²
#define HOME_SPEED          300     // steps/sec during homing
#define MAX_STEPS           20000   // full travel in steps (for position tracking)

// ── Microstepping (set to match MS1/MS2/MS3 jumpers on A4988) ────────────────
// This value is informational only — the firmware doesn't set the pins.
// Full step = 1, Half = 2, Quarter = 4, Eighth = 8, Sixteenth = 16
#define MICROSTEP_DIVISOR   1

#endif // CONFIG_H
