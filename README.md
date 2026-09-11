#  Multipurpose Agri-Bot

A modular, Bluetooth-controlled agricultural robot built on an **Arduino Mega 2560**. Designed and fabricated as a 4-wheel platform that combines mobility with six field operations — grass cutting, weeding, fertilizing, seed dispensing, height adjustment, and a dedicated scissor mechanism for harvesting.

![Agri-Bot](images/agribot_photo.png)

## Overview

The Agri-Bot is a ground-up mechatronics build: chassis fabrication, motor/driver selection, power distribution, and embedded control, all done independently. It's controlled wirelessly over Bluetooth, allowing remote operation from up to 10 meters away. The modular architecture makes it easy to swap implements and adapt to different agricultural tasks.

**Key result:** Reduced manual labour by ~60% during prototype field testing (ploughing/irrigation-prep tasks timed against manual work).

## Features

- **4-wheel skid-steer drive** — independent left/right motor pairing for tank-style turning
- **Cutter motor** — for grass/vegetation clearing
- **Water pump** — for irrigation
- **Lead screw mechanism** — motorized extend/retract for soil prep / attachment positioning
- **Scissor mechanism** — dedicated driver for a cutting/harvesting attachment, independent of the shared implement driver
- **Bluetooth wireless control** (HC-05) — joystick for driving, dedicated buttons for implements
- **High-current motor drivers (BTS7960)** — one pair per wheel side, one shared driver relay-switched across cutter/pump/lead-screw, and one dedicated driver for the scissor
- **Connection failsafe** — motors auto-stop if the Bluetooth link drops

## Hardware

| Component | Qty | Purpose |
|---|---|---|
| Arduino Mega 2560 | 1 | Main controller |
| BTS7960 motor driver | 4 | 2x wheel pairs + 1x shared implement driver + 1x dedicated scissor driver |
| HC-05 Bluetooth module | 1 | Wireless control link (on `Serial1`) |
| 6-channel relay module | 1 | Selects which implement is connected to the shared aux driver |
| DC gear motors (wheels) | 4 | Drive, wired as left pair / right pair |
| DC motor (grass cutter) | 1 | Grass cutting attachment |
| DC motor (weeder) | 1 | Weed removal attachment |
| DC motor / pump (fertilizing) | 1 | Fertilizer dispensing |
| Motor/solenoid (seed gate) | 1 | Opens/closes seed release gate |
| DC motor (seed rotation) | 1 | Drives seed-dispensing drum |
| DC motor (lead screw / height) | 1 | Linear extend/retract for frame height |
| DC motor (scissor) | 1 | Dedicated cutting/harvesting mechanism |
| Li-ion battery packs | 2 | Power supply (48V total) |
| Chassis (fabricated) | 1 | Modeled in CAD, built in-house |

## Wiring / Pin Map

| Signal | Mega Pin | Notes |
|---|---|---|
| HC-05 RX/TX | `Serial1` (18/19) | HC-05 RX needs a voltage divider (3.3V logic) |
| Left driver RPWM / LPWM / EN | 2 / 3 / 22 | Drives left wheel pair |
| Right driver RPWM / LPWM / EN | 4 / 5 / 23 | Drives right wheel pair |
| Aux driver RPWM / LPWM / EN | 6 / 7 / 24 | Shared across grass cutter/fertilizing/height/weeder/seed gate/seed rotation |
| Scissor driver RPWM / LPWM / EN | 8 / 9 / 25 | Dedicated — not on the aux relay |
| Relay — Grass cutter | 26 | Active-LOW relay module |
| Relay — Fertilizing | 27 | Active-LOW relay module |
| Relay — Height (lead screw) | 28 | Active-LOW relay module |
| Relay — Weeder | 29 | Active-LOW relay module |
| Relay — Seed gate | 30 | Active-LOW relay module |
| Relay — Seed rotation | 31 | Active-LOW relay module |

> The shared aux BTS7960 drives whichever implement's relay is closed — only one relay is ever active at a time, so grass cutter, fertilizing, height, weeder, seed gate, and seed rotation never run simultaneously.

### Hardware Connection Details

#### Motor Driver Configuration (BTS7960)

Each BTS7960 module has three control pins:
- **RPWM** (Right PWM): Controls forward rotation
- **LPWM** (Left PWM): Controls backward rotation
- **EN** (Enable): Global enable/disable for the driver

**Connection Steps:**
1. Connect RPWM and LPWM pins to Arduino PWM pins (2-13)
2. Connect EN pin to a standard digital pin for on/off control
3. Ensure a common ground between Arduino and BTS7960
4. Supply 12V-48V to the motor power pins depending on motor specifications

#### Relay Module Wiring

The 6-channel relay module operates with **active-LOW logic**:
- **Signal pin LOW** → Relay **CLOSES** (implement runs)
- **Signal pin HIGH** → Relay **OPENS** (implement stops)

**Connection:**
1. Connect each relay signal pin to Arduino pins 26-31
2. Connect relay GND to Arduino GND
3. Connect relay VCC to Arduino 5V
4. Each relay output connects to one implement motor through the aux driver

#### HC-05 Bluetooth Module

- **TX pin** → Arduino RX1 (Pin 19)
- **RX pin** → Arduino TX1 (Pin 18) **through a voltage divider** (5V to 3.3V)
  - Voltage divider: Use 1kΩ and 2kΩ resistors to drop 5V → 3.3V
- **VCC** → Arduino 5V
- **GND** → Arduino GND

#### Power Distribution

- **Main battery (48V):** Powers all BTS7960 drivers
- **5V buck converter:** Powers Arduino, HC-05, and relay module
- **Ground plane:** Ensure all ground connections are properly distributed to avoid noise issues

## Control Scheme

Controlled via a custom **MIT App Inventor** Bluetooth app over the HC-05 link.

**Drive:**

| Command | Action |
|---|---|
| `F` / `B` | Forward / Backward |
| `L` / `R` | Spin left / right |
| `S` | Stop |

**Implements (shared aux driver, one active at a time):**

| Button | Press sends | Release sends | Action |
|---|---|---|---|
| Grass Cutter ON / OFF | `1` | `q` | Toggle grass cutter |
| Fertilizing ON / OFF | `2` | `p` | Toggle fertilizing |
| Weeder ON / OFF | `w` | `v` | Toggle weeder |
| Height Up | `3` | `e` | Raise frame (momentary — runs while held) |
| Height Down | `4` | `r` | Lower frame (momentary — runs while held) |
| Seed Gate | `g` | — | Single button, toggles gate open/closed |
| Seed Rotation | `o` | — | Single button, toggles seed drum on/off |

**Scissor (dedicated driver, independent of aux):**

| Button | Press sends | Release sends | Action |
|---|---|---|---|
| Expand Scissor | `5` | `x` | Momentary — runs while held |
| Retract Scissor | `6` | `y` | Momentary — runs while held |

> ⚠️ These command characters are what the firmware expects. Double-check each button's `BluetoothClient1.Send1Text` block in App Inventor sends the matching character.

## Design

The chassis and mounting brackets were modeled in CAD before fabrication — a 4-wheel base frame with a raised height-adjustable deck carrying the seed hopper/tube assembly, and a folding X-frame for deployment flexibility.

## Repository Structure

```
Multipurpose-Agri-bot/
├── src/
│   └── AgriBot_Mega2560.ino   # Main firmware
├── images/
│   └── agribot_photo.png      # Build photo
├── docs/
│   └── wiring_notes.md        # Extra wiring/build notes
└── README.md
```

## Getting Started

1. Wire the components per the pin map above.
2. Open `src/AgriBot_Mega2560.ino` in the Arduino IDE.
3. Select **Board → Arduino Mega or Mega 2560** and the correct COM port.
4. Upload the sketch.
5. Pair your phone with the HC-05 (default PIN usually `1234` or `0000`).
6. Open the App Inventor app, confirm each button sends the command characters listed above, connect, and drive.

## Future Improvements

- [ ] Add limit switches on the lead screw / height mechanism to auto-stop at full travel
- [ ] Add current sensing on the BTS7960 modules for stall/overload protection
- [ ] Onboard soil-moisture or camera-based autonomy for reduced manual driving

## Author

Harishwaran T

Kishore K I

Sri Raam H M

## License

This project is licensed under the MIT License — see [LICENSE](LICENSE) for details.
