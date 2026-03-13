# mic-slider

WiFi-controlled stepper-motor microphone slider system.

An **Arduino Uno R4 WiFi** + **TB6600 driver** controls a **NEMA11 50mm linear lead-screw slide**. A native **macOS SwiftUI app** lets you jog sliders left/right, set speed, home them, and optionally show an IP camera feed alongside each one. Designed to start with one slider and scale to more.

---

## Parts List

| Item | Notes |
|---|---|
| Arduino Uno R4 WiFi (ABX00087) | Built-in WiFi — no shield needed |
| TB6600 stepper driver (×2) | 4A, 9–42V; one per slider |
| NEMA11 50mm linear slide (24V, 1.8°) | CNC lead-screw slide, T-shaped screw |
| 24V DC power supply, 5A | Recommended: 24V 5A switching supply (~$12 on Amazon); powers both sliders |
| Limit switches (×2 per slider) | Wire NC between pin and GND |
| Dupont jumper wires | Arduino → TB6600 signal wiring |
| Adafruit Motor Shield v2 | **Not used** — underpowered for 24V; TB6600 is wired direct to Arduino |

---

## Wiring

### TB6600 → Arduino (single-ended mode)

| TB6600 terminal | Connect to |
|---|---|
| PUL+ | Arduino pin 3 (STEP) |
| PUL− | GND |
| DIR+ | Arduino pin 4 (DIR) |
| DIR− | GND |
| ENA+ / ENA− | **Leave disconnected** — motor stays enabled and holds position |
| VMOT | 24V supply + |
| GND | 24V supply − |
| A+/A−/B+/B− | NEMA11 stepper motor coils |

### Limit switches

| Switch | Arduino pin |
|---|---|
| Left limit | Pin 6 (INPUT_PULLUP — wire switch between pin and GND) |
| Right limit | Pin 7 (INPUT_PULLUP — wire switch between pin and GND) |

### Power

- **24V supply** → TB6600 VMOT/GND only
- **Arduino** powered separately via USB or barrel jack (do not connect 24V to Arduino)

---

## Microstepping (TB6600 DIP switches)

Set on the TB6600 hardware — not in firmware. Recommended: **1/8 step**.

| Microstepping | SW1 | SW2 | SW3 | Steps/rev |
|---|---|---|---|---|
| Full | OFF | OFF | OFF | 200 |
| 1/2 | ON | OFF | OFF | 400 |
| 1/4 | OFF | ON | OFF | 800 |
| **1/8** | **ON** | **ON** | **OFF** | **1600** |
| 1/16 | OFF | OFF | ON | 3200 |

> Note: DIP switch labeling varies by TB6600 unit — check your specific board's manual.

### MAX_STEPS calibration (50mm slide)

Lead screw pitch on these NEMA11 slides is typically **2mm/revolution**.

| Microstepping | Steps/rev | Steps/mm | Full 50mm travel |
|---|---|---|---|
| Full | 200 | 100 | 5,000 |
| 1/2 | 400 | 200 | 10,000 |
| 1/4 | 800 | 400 | 20,000 |
| **1/8** | **1600** | **800** | **40,000** |

`MAX_STEPS` in `config.h` is set to `40000` (1/8 step, 2mm pitch). **Verify against your actual hardware** — jog to one end, home, jog to the other end, and read the position from `/status`.

---

## Firmware Setup

### Arduino IDE

1. **Boards Manager** → install **"Arduino UNO R4 Boards"**
2. Select board: **Arduino UNO R4 WiFi**
3. **Library Manager** → install **AccelStepper** by Mike McCauley
4. `WiFiS3` ships with the R4 board package — no separate install

### Configuration (`firmware/config.h`)

```cpp
#define WIFI_SSID   "YourSSID"
#define WIFI_PASS   "YourPassword"
#define STATIC_IP   IPAddress(192, 168, 1, 42)
#define GATEWAY_IP  IPAddress(192, 168, 1,  1)
#define SUBNET_MASK IPAddress(255, 255, 255, 0)
```

Static IP is strongly recommended — avoids DHCP delays and keeps app config stable.

---

## HTTP Endpoints

All `GET`, JSON responses.

| Endpoint | Action |
|---|---|
| `GET /status` | `{"pos":0,"moving":false,"maxSteps":40000,"jogDir":"none","homing":false,"speed":1600}` |
| `GET /move?dir=left&steps=200` | Move N steps (`left` or `right`) |
| `GET /jog/start?dir=left` | Begin continuous movement |
| `GET /jog/stop` | Stop immediately |
| `GET /home` | Run left to limit switch, zero position |
| `GET /speed?val=1600` | Set max speed (steps/sec, 1–10000) |

---

## macOS App

**Requirements:** macOS 13 Ventura+, Xcode 15+

Open `MicSlider/MicSlider.xcodeproj` → select **My Mac** → ⌘R.

Click **+** in the sidebar to add a slider by name and IP address.

### UI

```
┌─────────────────┬──────────────────────────────────────┐
│  Sliders        │  Stage Left Mic                      │
│  ─────────      │  IP: 192.168.1.42  [●] Connected     │
│  Stage Left Mic │                                      │
│  + Add Slider   │  Position: ▓▓▓▓▓░░░░░  4200/40000   │
│                 │                                      │
│                 │  [◄◄ LEFT]          [RIGHT ►►]       │
│                 │  tap = step, hold = continuous jog   │
│                 │                                      │
│                 │  Step size:  Fine | Med | Coarse      │
│                 │  Speed: ────●──────── 1600 steps/s   │
│                 │                                      │
│                 │  [⌂ Home]                            │
│                 │                                      │
│                 │  [Camera Feed ▼]  (optional)         │
└─────────────────┴──────────────────────────────────────┘
```

- **Tap** → moves by step size (Fine=50, Med=200, Coarse=800 steps)
- **Long press** → continuous jog; **Stop** button appears while moving
- **Speed** slider sends `/speed` to Arduino on release
- **Status** polled every 2 seconds
- Config persists to `UserDefaults` across launches

---

## Verification

```bash
# Firmware responding
curl http://192.168.1.42/status

# Move right 500 steps
curl "http://192.168.1.42/move?dir=right&steps=500"

# Jog left, then stop
curl "http://192.168.1.42/jog/start?dir=left"
curl "http://192.168.1.42/jog/stop"

# Home (runs to left limit switch, zeros position)
curl http://192.168.1.42/home

# Change speed
curl "http://192.168.1.42/speed?val=800"
```

---

## Scaling to Multiple Sliders

Each Arduino gets a unique static IP in `config.h`. Add each one in the app sidebar. Two TB6600 drivers + a 24V 5A supply handles both sliders simultaneously.

---

## Roadmap

- [ ] Order 24V 5A power supply
- [ ] Flash firmware, connect to WiFi, verify with `curl /status`
- [ ] Wire TB6600 + limit switches, test homing
- [ ] Calibrate `MAX_STEPS` against actual slide travel
- [ ] Build macOS app in Xcode, add slider by IP, verify jog end-to-end
- [ ] Add second slider
