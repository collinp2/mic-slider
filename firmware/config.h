// config.h — WiFi and pin configuration for mic-slider firmware
// Arduino Uno R4 WiFi (ABX00087)
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

#endif // CONFIG_H
