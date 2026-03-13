# mic-slider

WiFi-controlled stepper-motor microphone slider system.

An **Arduino Uno R4 WiFi** + **A4988 driver** sits on each slider and runs a small HTTP server. A native **macOS SwiftUI app** lets you jog sliders left/right, set speed, home them, and optionally show an IP camera feed alongside each one. Designed to start with one slider and scale to more.

---

## Hardware

### What you need

| Item | Notes |
|---|---|
| **Arduino Uno R4 WiFi (ABX00087)** | Built-in WiFi — no shield needed |
| **A4988 stepper driver** | Standard breakout board |
| **100µF electrolytic capacitor** | Across VMOT/GND on A4988 — prevents voltage spikes that kill the chip |
| **Limit switches ×2 per slider** | Wired NC to GND; recommend cheap microswitches |
| **12V DC power supply** | For stepper motor via A4988 VMOT |
| **IP camera (optional)** | Any camera with MJPEG or HLS stream URL (e.g. Reolink, TP-Link Tapo) |

### Wiring (A4988 pin assignments)

| Signal | Arduino Pin |
|---|---|
| STEP | 3 |
| DIR | 4 |
| ENABLE | 5 |
| Limit switch LEFT | 6 (INPUT_PULLUP) |
| Limit switch RIGHT | 7 (INPUT_PULLUP) |

**Important:** Put a 100µF cap across A4988 VMOT/GND. Power the Arduino via USB or barrel jack separately from the 12V motor supply.

---

## Architecture

```
[Arduino Uno R4 WiFi + A4988]       [macOS SwiftUI App]
  AccelStepper library         <-->  URLSession HTTP calls
  HTTP server on port 80             Jog buttons / position display
  Static IP (no DHCP)                Sidebar for multiple sliders
  Limit switch safety checks         Optional IP camera feed per slider
```

---

## Project Layout

```
mic-slider/
  firmware/
    mic-slider.ino      ← Arduino sketch (WiFiS3 + AccelStepper)
    config.h            ← WiFi SSID/password, static IP, pin assignments
  MicSlider/
    MicSlider.xcodeproj/
    MicSlider/
      MicSliderApp.swift      ← App entry point (macOS 13+)
      ContentView.swift       ← NavigationSplitView: sidebar + detail
      SliderDetailView.swift  ← Jog controls, position bar, speed, camera
      SliderStore.swift       ← ObservableObject: slider list + UserDefaults persistence
      SliderClient.swift      ← URLSession HTTP wrapper (actor)
      CameraFeedView.swift    ← WKWebView MJPEG/HLS feed
```

---

## Firmware

### Arduino IDE Setup

1. **Boards Manager** → install **"Arduino UNO R4 Boards"**
2. Select board: **Arduino UNO R4 WiFi**
3. **Library Manager** → install **AccelStepper** by Mike McCauley
4. `WiFiS3` is included with the R4 board package — no separate install needed

### Configuration

Edit `firmware/config.h` before flashing:

```cpp
#define WIFI_SSID   "YourSSID"
#define WIFI_PASS   "YourPassword"
#define STATIC_IP   IPAddress(192, 168, 1, 42)
#define GATEWAY_IP  IPAddress(192, 168, 1,  1)
#define SUBNET_MASK IPAddress(255, 255, 255, 0)
```

Static IP is strongly recommended — avoids DHCP lookup delays and keeps the app config stable.

### HTTP Endpoints

All `GET`, plain JSON responses.

| Endpoint | Action |
|---|---|
| `GET /status` | `{"pos":1234,"moving":false,"maxSteps":20000,"jogDir":"none","homing":false,"speed":800}` |
| `GET /move?dir=left&steps=200` | Move N steps in direction (`left` or `right`) |
| `GET /jog/start?dir=left` | Begin continuous movement |
| `GET /jog/stop` | Stop immediately |
| `GET /home` | Run left to limit switch, zero position |
| `GET /speed?val=800` | Set max speed (steps/sec, 1–10000) |

### Key firmware behaviors

- **AccelStepper** handles acceleration/deceleration automatically
- **Jog:** sets a target 1,000,000 steps away; `/jog/stop` interrupts it
- **Homing:** runs left at `HOME_SPEED` until left limit switch fires, then zeros position and restores full speed
- **Limit switches** checked every loop iteration — stops immediately if triggered during any move
- **Motor disabled** (ENABLE pin HIGH) when idle to reduce heat

---

## macOS App

**Requirements:** macOS 13 Ventura+, Xcode 15+

### Build & Run

1. Open `MicSlider/MicSlider.xcodeproj` in Xcode
2. Select **My Mac** as the run destination
3. Build & Run (⌘R)

### Adding a Slider

Click **+** in the sidebar → enter a name, IP address, and optional camera stream URL.

Config persists to `UserDefaults` across launches.

### UI Overview

```
┌─────────────────┬──────────────────────────────────────┐
│  Sliders        │  Stage Left Mic                      │
│  ─────────      │  IP: 192.168.1.42  [●] Connected     │
│  Stage Left Mic │                                      │
│  + Add Slider   │  Position: ▓▓▓▓▓░░░░░  4200/20000   │
│                 │                                      │
│                 │  [◄◄ LEFT]          [RIGHT ►►]       │
│                 │  tap = step, hold = continuous jog   │
│                 │                                      │
│                 │  Step size:  Fine | Med | Coarse      │
│                 │  Speed: ────●──────── 800 steps/s    │
│                 │                                      │
│                 │  [⌂ Home]                            │
│                 │                                      │
│                 │  [Camera Feed ▼]                     │
│                 │  ┌──────────────────────────────┐    │
│                 │  │  live MJPEG/HLS feed          │    │
│                 │  └──────────────────────────────┘    │
└─────────────────┴──────────────────────────────────────┘
```

- **Tap** jog button → moves by selected step size (Fine=50, Med=200, Coarse=800 steps)
- **Long press** jog button → continuous jog until you click **Stop**
- **Stop** button appears automatically while slider is moving
- **Speed** slider updates the Arduino in real time on release
- **Camera feed** is collapsible; leave camera URL blank to hide it
- **Status polling** every 2 seconds while a slider is selected

---

## Verification Checklist

```bash
# 1. Firmware responding
curl http://192.168.1.42/status

# 2. Motor moves
curl "http://192.168.1.42/move?dir=right&steps=500"

# 3. Jog
curl "http://192.168.1.42/jog/start?dir=left"
curl "http://192.168.1.42/jog/stop"

# 4. Homing
curl http://192.168.1.42/home

# 5. Speed change
curl "http://192.168.1.42/speed?val=1200"
```

---

## Scaling to Multiple Sliders

Each Arduino gets a unique static IP in `config.h`. Add each one in the app sidebar by IP. They are controlled completely independently.

---

## Roadmap / Next Steps

- [ ] Flash firmware to first R4 WiFi board and verify with curl
- [ ] Wire up A4988 + stepper + limit switches, test homing
- [ ] Build macOS app in Xcode, add slider by IP, verify jog end-to-end
- [ ] Add second slider — confirm multi-slider sidebar works
- [ ] Add IP camera URL if using camera feed
- [ ] Consider a `/config` endpoint to read pin assignments / max steps remotely
