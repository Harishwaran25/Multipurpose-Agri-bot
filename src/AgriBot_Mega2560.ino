/*
  ===========================================================================
  MULTIPURPOSE AGRI-BOT — Arduino Mega 2560
  ===========================================================================
  Control: Bluetooth (HC-05) via a custom MIT App Inventor app.
           - Forward / Backward / Left / Right / Stop -> discrete drive buttons
           - Expand Scissor / Retract Scissor  -> dedicated scissor driver
           - Height Up / Height Down           -> lead-screw height actuator
           - Fertilizing ON / OFF              -> aux driver, relay-selected
           - Weeder ON / OFF                   -> aux driver, relay-selected
           - Grass Cutter ON / OFF             -> aux driver, relay-selected
           - Seed Gate (single button, toggles open/closed)
           - Seed Rotation (single button, toggles on/off)

  Drivers: 4x BTS7960 high-current motor drivers
           #1 -> Left wheel pair (2 motors wired in parallel)
           #2 -> Right wheel pair (2 motors wired in parallel)
           #3 -> Shared "aux" driver. A 6-channel relay module selects which
                 single motor (grass cutter / fertilizing pump / height
                 lead-screw / weeder / seed gate / seed rotation) is
                 actually connected to its output at any given time — only
                 one relay is ever closed at once, so only one aux
                 implement runs at once.
           #4 -> Dedicated driver for the scissor mechanism (separate
                 implement, not routed through the aux relay selector — can
                 run at the same time as an aux implement).

  HC-05:   Wired to Serial1 (TX1=pin18, RX1=pin19) — Mega has 4 hardware
           UARTs, so no SoftwareSerial needed. Wire HC-05 TX -> Mega RX1,
           HC-05 RX -> Mega TX1 through a voltage divider (HC-05 RX is 3.3V
           logic; Mega TX is 5V — protect it with a 1k/2k divider or a
           logic-level shifter).
  ===========================================================================
*/

// ---------- LEFT WHEEL DRIVER (BTS7960 #1) ----------
const int LEFT_RPWM = 2;
const int LEFT_LPWM = 3;
const int LEFT_EN   = 22;   // tie BTS7960 R_EN + L_EN together to this pin

// ---------- RIGHT WHEEL DRIVER (BTS7960 #2) ----------
const int RIGHT_RPWM = 4;
const int RIGHT_LPWM = 5;
const int RIGHT_EN   = 23;

// ---------- SHARED AUX DRIVER (BTS7960 #3) ----------
const int AUX_RPWM = 6;
const int AUX_LPWM = 7;
const int AUX_EN   = 24;

// ---------- SCISSOR DRIVER (BTS7960 #4, dedicated — not on aux relay) ----------
const int SCISSOR_RPWM = 8;
const int SCISSOR_LPWM = 9;
const int SCISSOR_EN   = 25;   // tie BTS7960 R_EN + L_EN together to this pin

// ---------- RELAYS: select which implement is on the aux driver ----------
const int RELAY_GRASSCUTTER  = 26;
const int RELAY_FERTILIZING  = 27;
const int RELAY_HEIGHT       = 28;
const int RELAY_WEEDER       = 29;
const int RELAY_SEEDGATE     = 30;
const int RELAY_SEEDROTATION = 31;
// NOTE: most relay modules are ACTIVE-LOW (LOW = relay closed/ON).
// If yours is active-HIGH, swap RELAY_ON/RELAY_OFF below.
const int RELAY_ON  = LOW;
const int RELAY_OFF = HIGH;

// ---------- SPEEDS (0-255) ----------
const int DRIVE_SPEED   = 180;   // wheel motor speed
const int AUX_SPEED     = 200;   // shared aux implement speed
const int SCISSOR_SPEED = 200;   // scissor expand/retract speed

// ---------- STATE (only needed for the single-button toggle implements) ----------
bool seedGateOn     = false;
bool seedRotationOn = false;

// Failsafe: stop everything if no command received for this long (ms)
const unsigned long CMD_TIMEOUT = 1000;
unsigned long lastCmdTime = 0;

void setup() {
  Serial.begin(9600);     // USB debug
  Serial1.begin(9600);    // HC-05 (match your module's baud rate)

  pinMode(LEFT_RPWM, OUTPUT);    pinMode(LEFT_LPWM, OUTPUT);    pinMode(LEFT_EN, OUTPUT);
  pinMode(RIGHT_RPWM, OUTPUT);   pinMode(RIGHT_LPWM, OUTPUT);   pinMode(RIGHT_EN, OUTPUT);
  pinMode(AUX_RPWM, OUTPUT);     pinMode(AUX_LPWM, OUTPUT);     pinMode(AUX_EN, OUTPUT);
  pinMode(SCISSOR_RPWM, OUTPUT); pinMode(SCISSOR_LPWM, OUTPUT); pinMode(SCISSOR_EN, OUTPUT);

  pinMode(RELAY_GRASSCUTTER, OUTPUT);
  pinMode(RELAY_FERTILIZING, OUTPUT);
  pinMode(RELAY_HEIGHT, OUTPUT);
  pinMode(RELAY_WEEDER, OUTPUT);
  pinMode(RELAY_SEEDGATE, OUTPUT);
  pinMode(RELAY_SEEDROTATION, OUTPUT);

  digitalWrite(LEFT_EN, HIGH);
  digitalWrite(RIGHT_EN, HIGH);
  digitalWrite(AUX_EN, HIGH);
  digitalWrite(SCISSOR_EN, HIGH);

  stopAll();
  lastCmdTime = millis();
}

void loop() {
  if (Serial1.available()) {
    char cmd = Serial1.read();
    lastCmdTime = millis();
    handleCommand(cmd);
  }

  // Failsafe: if Bluetooth link drops mid-drive, stop wheels
  if (millis() - lastCmdTime > CMD_TIMEOUT) {
    driveLeft(0);
    driveRight(0);
  }
}

void handleCommand(char cmd) {
  switch (cmd) {
    // ---- Drive (discrete buttons) ----
    case 'F': driveLeft(DRIVE_SPEED);  driveRight(DRIVE_SPEED);  break; // forward
    case 'B': driveLeft(-DRIVE_SPEED); driveRight(-DRIVE_SPEED); break; // backward
    case 'L': driveLeft(-DRIVE_SPEED); driveRight(DRIVE_SPEED);  break; // spin left
    case 'R': driveLeft(DRIVE_SPEED);  driveRight(-DRIVE_SPEED); break; // spin right
    case 'S': driveLeft(0); driveRight(0); break;                      // stop

    // ---- Grass Cutter ON / OFF ----
    case '1': selectAux(RELAY_GRASSCUTTER); auxRun(AUX_SPEED); break; // ON
    case 'q': auxRun(0); allRelaysOff(); break;                        // OFF

    // ---- Fertilizing ON / OFF ----
    case '2': selectAux(RELAY_FERTILIZING); auxRun(AUX_SPEED); break; // ON
    case 'p': auxRun(0); allRelaysOff(); break;                        // OFF

    // ---- Weeder ON / OFF ----
    case 'w': selectAux(RELAY_WEEDER); auxRun(AUX_SPEED); break;       // ON
    case 'v': auxRun(0); allRelaysOff(); break;                        // OFF

    // ---- Height Up / Down (momentary — set app to send '3'/'4' on press
    //      and 'e'/'r' on release, runs while held) ----
    case '3': selectAux(RELAY_HEIGHT); auxRun(AUX_SPEED);  break; // up (extend)
    case 'e': auxRun(0); allRelaysOff(); break;
    case '4': selectAux(RELAY_HEIGHT); auxRun(-AUX_SPEED); break; // down (retract)
    case 'r': auxRun(0); allRelaysOff(); break;

    // ---- Scissor Expand / Retract (dedicated driver, momentary —
    //      '5'/'6' on press, 'x'/'y' on release) ----
    case '5': scissorRun(SCISSOR_SPEED);  break; // expand
    case 'x': scissorRun(0); break;
    case '6': scissorRun(-SCISSOR_SPEED); break; // retract
    case 'y': scissorRun(0); break;

    // ---- Seed Gate (single button — toggles open/closed) ----
    case 'g':
      seedGateOn = !seedGateOn;
      if (seedGateOn) { selectAux(RELAY_SEEDGATE); auxRun(AUX_SPEED); }
      else            { auxRun(0); allRelaysOff(); }
      break;

    // ---- Seed Rotation (single button — toggles on/off) ----
    case 'o':
      seedRotationOn = !seedRotationOn;
      if (seedRotationOn) { selectAux(RELAY_SEEDROTATION); auxRun(AUX_SPEED); }
      else                 { auxRun(0); allRelaysOff(); }
      break;

    default:
      break; // ignore unknown bytes
  }
}

// ---------- LOW-LEVEL MOTOR HELPERS ----------

void driveLeft(int speed) {
  if (speed > 0)      { analogWrite(LEFT_RPWM, speed);  analogWrite(LEFT_LPWM, 0); }
  else if (speed < 0) { analogWrite(LEFT_RPWM, 0);       analogWrite(LEFT_LPWM, -speed); }
  else                { analogWrite(LEFT_RPWM, 0);       analogWrite(LEFT_LPWM, 0); }
}

void driveRight(int speed) {
  if (speed > 0)      { analogWrite(RIGHT_RPWM, speed);  analogWrite(RIGHT_LPWM, 0); }
  else if (speed < 0) { analogWrite(RIGHT_RPWM, 0);       analogWrite(RIGHT_LPWM, -speed); }
  else                { analogWrite(RIGHT_RPWM, 0);       analogWrite(RIGHT_LPWM, 0); }
}

void auxRun(int speed) {
  if (speed > 0)      { analogWrite(AUX_RPWM, speed);  analogWrite(AUX_LPWM, 0); }
  else if (speed < 0) { analogWrite(AUX_RPWM, 0);       analogWrite(AUX_LPWM, -speed); }
  else                { analogWrite(AUX_RPWM, 0);       analogWrite(AUX_LPWM, 0); }
}

void scissorRun(int speed) {
  if (speed > 0)      { analogWrite(SCISSOR_RPWM, speed);  analogWrite(SCISSOR_LPWM, 0); }
  else if (speed < 0) { analogWrite(SCISSOR_RPWM, 0);       analogWrite(SCISSOR_LPWM, -speed); }
  else                { analogWrite(SCISSOR_RPWM, 0);       analogWrite(SCISSOR_LPWM, 0); }
}

void selectAux(int relayPin) {
  allRelaysOff();          // make sure only one path is ever live
  digitalWrite(relayPin, RELAY_ON);
}

void allRelaysOff() {
  digitalWrite(RELAY_GRASSCUTTER, RELAY_OFF);
  digitalWrite(RELAY_FERTILIZING, RELAY_OFF);
  digitalWrite(RELAY_HEIGHT, RELAY_OFF);
  digitalWrite(RELAY_WEEDER, RELAY_OFF);
  digitalWrite(RELAY_SEEDGATE, RELAY_OFF);
  digitalWrite(RELAY_SEEDROTATION, RELAY_OFF);
}

void stopAll() {
  driveLeft(0);
  driveRight(0);
  auxRun(0);
  scissorRun(0);
  allRelaysOff();
  seedGateOn = false;
  seedRotationOn = false;
}
