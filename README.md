# mic-slider

WiFi-controlled stepper-motor microphone slider system.

An **Arduino Uno R4 WiFi** + **3× Adafruit Motor Shield v2** (stacked) controls up to **6 NEMA11 100mm linear lead-screw slides**. A native **macOS SwiftUI app** lets you jog sliders left/right, set speed, home them, and optionally show an IP camera feed alongside each one. Only one slider moves at a time.

---

## Parts List

| Item | Notes |
|---|---|
| Arduino Uno R4 WiFi (ABX00087) | Built-in WiFi — no shield needed |
| Adafruit Motor Shield v2 (×3) | Stacked via I2C; controls 2 steppers each = 6 total |
| NEMA11 linear slide, T6x1, 100mm (×6) | 28mm flange, 1mm pitch, 0.6A, 1.8° step angle — [Amazon $35.99](https://www.amazon.com) |
| 12V 2A power supply, center positive, 5.5mm×2.1mm barrel jack | Powers all 3 Motor Shields; coil release after each move prevents heat buildup |
| HiLetgo KW12-3 roller lever microswitches (×10 pack) | SPDT; 2 per slider for homing |
| TP-Link Tapo C110 IP camera | IR night vision, WiFi, RTSP stream; one per slider (optional) |
| Dupont jumper wires | Limit switch wiring |

---

## Motor Shield Stacking (I2C Address Configuration)

Solder address jumpers on the Motor Shield PCB before stacking:

| Shield | Address | Jumper |
|---|---|---|
| Shield 1 (bottom) | 0x60 | No jumpers (default) |
| Shield 2 (middle) | 0x61 | Solder **A0** |
| Shield 3 (top) | 0x62 | Solder **A1** |

Stack all three on the Arduino. They share SDA/SCL (I2C). No other connections needed between shields.

### Motor assignments

| Slider | Shield | Port |
|---|---|---|
| 1 | Shield 1 (0x60) | M1/M2 |
| 2 | Shield 1 (0x60) | M3/M4 |
| 3 | Shield 2 (0x61) | M1/M2 |
| 4 | Shield 2 (0x61) | M3/M4 |
| 5 | Shield 3 (0x62) | M1/M2 |
| 6 | Shield 3 (0x62) | M3/M4 |

Connect the 4 motor wires (Red=A+, Blue=A−, Green=B+, Black=B−) to each Motor Shield terminal block.

---

## Wiring

### Limit switches

| Slider | Left limit | Right limit |
|---|---|---|
| 1 | Pin 2 | Pin 3 |
| 2 | Pin 4 | Pin 5 |
| 3 | Pin 6 | Pin 7 |
| 4 | Pin 8 | Pin 9 |
| 5 | Pin 10 | Pin 11 |
| 6 | Pin 12 | Pin 13 |

Wire each switch between the pin and GND (INPUT_PULLUP: LOW = triggered).
Use **COM** and **NC (normally closed)** terminals on each KW12-3. Leave NO unconnected.

### Power

- **12V supply** → Motor Shield power terminals (M+ / GND screw block on bottom shield)
- **Arduino** powered separately via USB or barrel jack

> **Power supply:** Use a **12V 2A center-positive** adapter (5.5mm×2.1mm barrel). The Motor Shield v2 maximum input is 13.5V — do not use a 24V supply. The firmware releases motor coils after every move; the T6x1 lead screw is self-locking and will not back-drive, so heat is not a concern in normal use.

---

## Slide Specs & Step Math

| Spec | Value |
|---|---|
| Lead screw | T6x1 (1mm pitch — carriage moves 1mm per motor revolution) |
| Steps/revolution | 200 (1.8° step angle, DOUBLE mode) |
| Steps/mm | 200 |
| Full travel (100mm) | **20,000 steps** |
| Positioning accuracy | ±0.1mm (±20 steps) |

`MAX_STEPS` in `config.h` is set to `20000`. Verify against your actual hardware after first homing run.

---

## Firmware Setup

### Arduino IDE

1. **Boards Manager** → install **"Arduino UNO R4 Boards"**
2. Select board: **Arduino UNO R4 WiFi**
3. **Library Manager** → install:
   - **Adafruit Motor Shield V2 Library** by Adafruit
   - **AccelStepper** by Mike McCauley
4. `WiFiS3` ships with the R4 board package — no separate install

### Configuration (`firmware/config.h`)

```cpp
#define WIFI_SSID   "YourSSID"
#define WIFI_PASS   "YourPassword"
#define STATIC_IP   IPAddress(10, 0, 0, 42)
#define GATEWAY_IP  IPAddress(10, 0, 0,  1)
#define SUBNET_MASK IPAddress(255, 255, 255, 0)
```

Static IP is strongly recommended — avoids DHCP delays and keeps app config stable.

---

## HTTP Endpoints

All `GET`, JSON responses. All endpoints accept `?slider=1` through `?slider=6` (default: 1).

| Endpoint | Action |
|---|---|
| `GET /status` | `{"slider":1,"pos":0,"moving":false,"maxSteps":20000,"jogDir":"none","homing":false,"speed":2000}` |
| `GET /move?dir=left&steps=200` | Move N steps (`left` or `right`) |
| `GET /jog/start?dir=left` | Begin continuous movement |
| `GET /jog/stop` | Stop immediately |
| `GET /home` | Run left to limit switch, zero position |
| `GET /speed?val=1600` | Set max speed (steps/sec, 100–3000) |

---

## Position Persistence (EEPROM)

The firmware saves each slider's position to Arduino EEPROM after every move. On power-up, positions are restored — homing is not required every session. 6 sliders × 4 bytes = 24 bytes used.

The macOS app also stores `lastKnownPosition` per slider in UserDefaults and displays it even when the slider is offline.

---

## macOS App

**Requirements:** macOS 13 Ventura+, Xcode 15+

Open `MicSlider/MicSlider.xcodeproj` → select **My Mac** → ⌘R.

Click **+** in the sidebar to add a slider by name, IP address, and channel (1–6).

### UI

```
┌─────────────────┬──────────────────────────────────────┐
│  Sliders        │  Stage Left Mic                      │
│  ─────────      │  IP: 10.0.0.42:8080  ·  Slider 1    │
│  Stage Left Mic │  [●] Connected                       │
│  + Add Slider   │                                      │
│                 │  Position: ▓▓▓▓▓░░░░░  4200/20000   │
│                 │                                      │
│                 │  [◄◄ LEFT]          [RIGHT ►►]       │
│                 │  tap = step, hold = continuous jog   │
│                 │                                      │
│                 │  Step size:  Fine | Med | Coarse      │
│                 │  Speed: ────●──────── 2000 steps/s   │
│                 │                                      │
│                 │  [⌂ Home]                            │
│                 │                                      │
│                 │  [Camera Feed ▼]  (optional)         │
└─────────────────┴──────────────────────────────────────┘
```

- **Tap** → moves by step size (Fine=800, Med=2000, Coarse=4000 steps)
- **Long press** → continuous jog; **Stop** button appears while moving
- **Speed** slider sends `/speed` to Arduino on release
- **Status** polled after each move (calculated delay, no continuous polling)
- Config and last known position persist to `UserDefaults` across launches

---

## Verification

```bash
# Firmware responding
curl http://10.0.0.42:8080/status?slider=1

# Move right 200 steps
curl "http://10.0.0.42:8080/move?dir=right&steps=200&slider=1"

# Jog left, then stop
curl "http://10.0.0.42:8080/jog/start?dir=left&slider=1"
curl "http://10.0.0.42:8080/jog/stop?slider=1"

# Home slider 1
curl "http://10.0.0.42:8080/home?slider=1"

# Change speed
curl "http://10.0.0.42:8080/speed?val=1500&slider=1"
```

---

## Roadmap

- [x] Flash firmware, verify WiFi — IP: 10.0.0.42:8080
- [x] Build macOS app, verify jog end-to-end
- [x] Dual slider support (2 sliders, 1 Arduino)
- [x] Redesign for 3× Motor Shield v2 + 6× NEMA11 T6x1 100mm slides
- [ ] Order/receive 6 slides (ordered 2 to test first)
- [ ] Wire Motor Shields, flash new firmware, verify all 6 channels
- [ ] Wire limit switches, test homing
- [ ] Calibrate MAX_STEPS against actual slide travel
- [ ] Update CameraFeedView for Tapo C110 RTSP stream (AVFoundation)
- [ ] Add 6 Tapo C110 cameras (one per slider)
