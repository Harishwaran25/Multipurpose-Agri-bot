/*
  ===========================================================================
  MULTIPURPOSE AGRI-BOT — Arduino Mega 2560
  ===========================================================================
  Control: Bluetooth (HC-05) via "Arduino Bluetooth Controller" app (Broxcode)
           - Joystick pad  -> movement (skid-steer, left pair / right pair)
           - Buttons 1-4   -> cutter, pump, lead screw extend/retract

  Drivers: 3x BTS7960 high-current motor drivers
           #1 -> Left wheel pair (2 motors wired in parallel)
           #2 -> Right wheel pair (2 motors wired in parallel)
           #3 -> Shared "aux" driver. A 3-channel relay module selects which
                 single motor (cutter / pump / lead screw) is actually
                 connected to its output at any given time — only one relay
                 is ever closed at once, so only one aux motor runs at once.

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

// ---------- RELAYS: select which implement is on the aux driver ----------
const int RELAY_CUTTER    = 26;
const int RELAY_PUMP      = 27;
const int RELAY_LEADSCREW = 28;
// NOTE: most relay modules are ACTIVE-LOW (LOW = relay closed/ON).
// If yours is active-HIGH, swap RELAY_ON/RELAY_OFF below.
const int RELAY_ON  = LOW;
const int RELAY_OFF = HIGH;

// ---------- SPEEDS (0-255) ----------
const int DRIVE_SPEED = 180;   // wheel motor speed
const int AUX_SPEED   = 200;   // cutter/pump/lead-screw speed

// ---------- STATE ----------
bool cutterOn = false;
bool pumpOn   = false;

// Failsafe: stop everything if no command received for this long (ms)
const unsigned long CMD_TIMEOUT = 1000;
unsigned long lastCmdTime = 0;

void setup() {
  Serial.begin(9600);     // USB debug
  Serial1.begin(9600);    // HC-05 (match your module's baud rate)

  pinMode(LEFT_RPWM, OUTPUT);  pinMode(LEFT_LPWM, OUTPUT);  pinMode(LEFT_EN, OUTPUT);
  pinMode(RIGHT_RPWM, OUTPUT); pinMode(RIGHT_LPWM, OUTPUT); pinMode(RIGHT_EN, OUTPUT);
  pinMode(AUX_RPWM, OUTPUT);   pinMode(AUX_LPWM, OUTPUT);   pinMode(AUX_EN, OUTPUT);

  pinMode(RELAY_CUTTER, OUTPUT);
  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_LEADSCREW, OUTPUT);
  digitalWrite(RELAY_CUTTER, RELAY_OFF);
  digitalWrite(RELAY_PUMP, RELAY_OFF);
  digitalWrite(RELAY_LEADSCREW, RELAY_OFF);

  digitalWrite(LEFT_EN, HIGH);
  digitalWrite(RIGHT_EN, HIGH);
  digitalWrite(AUX_EN, HIGH);

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
    // ---- Joystick: movement (standard Bluetooth Controller app codes) ----
    case 'F': driveLeft(DRIVE_SPEED);  driveRight(DRIVE_SPEED);  break; // forward
    case 'B': driveLeft(-DRIVE_SPEED); driveRight(-DRIVE_SPEED); break; // backward
    case 'L': driveLeft(-DRIVE_SPEED); driveRight(DRIVE_SPEED);  break; // spin left
    case 'R': driveLeft(DRIVE_SPEED);  driveRight(-DRIVE_SPEED); break; // spin right
    case 'G': driveLeft(DRIVE_SPEED/2); driveRight(DRIVE_SPEED); break; // fwd-left
    case 'I': driveLeft(DRIVE_SPEED);  driveRight(DRIVE_SPEED/2); break; // fwd-right
    case 'H': driveLeft(-DRIVE_SPEED/2); driveRight(-DRIVE_SPEED); break; // back-left
    case 'J': driveLeft(-DRIVE_SPEED); driveRight(-DRIVE_SPEED/2); break; // back-right
    case 'S': driveLeft(0); driveRight(0); break; // stop (joystick released)

    // ---- Button 1: cutter toggle ----
    case '1':
      cutterOn = !cutterOn;
      if (cutterOn) { selectAux(RELAY_CUTTER); auxRun(AUX_SPEED); }
      else          { auxRun(0); allRelaysOff(); }
      break;

    // ---- Button 2: pump toggle ----
    case '2':
      pumpOn = !pumpOn;
      if (pumpOn) { selectAux(RELAY_PUMP); auxRun(AUX_SPEED); }
      else        { auxRun(0); allRelaysOff(); }
      break;

    // ---- Button 3: lead screw EXTEND (momentary — set app to send '3'
    //      on press and 'e' on release) ----
    case '3':
      selectAux(RELAY_LEADSCREW);
      auxRun(AUX_SPEED);
      break;
    case 'e':
      auxRun(0);
      allRelaysOff();
      break;

    // ---- Button 4: lead screw RETRACT (momentary — '4' on press,
    //      'r' on release) ----
    case '4':
      selectAux(RELAY_LEADSCREW);
      auxRun(-AUX_SPEED);
      break;
    case 'r':
      auxRun(0);
      allRelaysOff();
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

void selectAux(int relayPin) {
  allRelaysOff();          // make sure only one path is ever live
  digitalWrite(relayPin, RELAY_ON);
}

void allRelaysOff() {
  digitalWrite(RELAY_CUTTER, RELAY_OFF);
  digitalWrite(RELAY_PUMP, RELAY_OFF);
  digitalWrite(RELAY_LEADSCREW, RELAY_OFF);
}

void stopAll() {
  driveLeft(0);
  driveRight(0);
  auxRun(0);
  allRelaysOff();
  cutterOn = false;
  pumpOn = false;
}
