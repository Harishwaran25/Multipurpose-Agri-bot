# Wiring Notes

## Power
- Two Li-ion battery packs power the motor drivers and logic separately (recommended: keep motor power and Arduino/logic power on separate rails with a common ground, to avoid brownouts on the Mega when motors draw high current).
- BTS7960 modules are rated for high current — suitable for the wheel motors and the shared implement motors.

## HC-05 Bluetooth Module
- Connected to `Serial1` on the Mega (RX1 = pin 19, TX1 = pin 18) instead of `SoftwareSerial`, since the Mega has multiple hardware UARTs available.
- **Important:** HC-05's RX pin is 3.3V logic. The Mega's TX1 outputs 5V, so a voltage divider (e.g. 1kΩ / 2kΩ) or a logic-level shifter is required between Mega TX1 and HC-05 RX to avoid damaging the module.
- HC-05 TX → Mega RX1 can be connected directly (5V tolerant on most boards' RX for reading 3.3V signals, but check your specific board revision).

## Relay Module (Aux Motor Selector)
- Confirm whether your relay module is **active-LOW** (most common — `LOW` closes the relay) or **active-HIGH** before flashing the firmware. This is set in `RELAY_ON` / `RELAY_OFF` at the top of the sketch.
- Only one relay should ever be closed at a time — this is enforced in software (`selectAux()` always calls `allRelaysOff()` first) but it's worth double-checking physically before running under load.

## BTS7960 Enable Pins
- `R_EN` and `L_EN` on each BTS7960 module are tied together and driven from a single Mega pin per driver (see pin map in the main README). They're set `HIGH` once in `setup()`.

## Scissor Driver (BTS7960 #4)
- Wired directly to a single motor — unlike the aux driver, it is **not** shared through the relay module, so the scissor can run at the same time as the cutter/pump/lead-screw.
- Uses its own RPWM/LPWM/EN pins (8/9/25) — chosen to avoid overlap with the wheel and aux drivers' pins (2-7, 22-24).

## Suggested Additions
- Limit switches at both ends of the lead screw travel, wired to interrupt pins, to auto-cut power on full extend/retract and prevent motor stall.
- A low-voltage cutoff or battery monitor to protect the Li-ion packs from over-discharge.
