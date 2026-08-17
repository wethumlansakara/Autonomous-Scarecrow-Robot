#include <SoftwareSerial.h>
#include <Servo.h>

SoftwareSerial espSerial(2, 4); // RX, TX
Servo myServo;

// PINS
const int PIR_PIN   = 11;
const int SERVO_PIN = 3;

// Ultrasonic
const int TRIG_PIN = 12;
const int ECHO_PIN = 13;
const int STOP_DISTANCE_CM = 20;

// Motors
const int IN1 = 10;
const int IN2 = 9;
const int IN3 = 8;
const int IN4 = 7;
const int ENA = 5;
const int ENB = 6;

// IR Sensors
const int IR_PINS[5] = {A0, A1, A2, A3, A4};
int sensorValues[5];

// Change to 1 if your black line gives HIGH
const int LINE_COLOUR = 0;

// PID
float Kp = 40.0; //50
float Ki = 0.0;
float Kd = 12.0; //7

float error = 0;
float lastError = 0;
float integral = 0;

// From ESP32/Firebase
char currentCommand = 'S';
char robotMode = 'A';
int currentSpeed = 90; //90
int currentServo = 1;

// Stop bar control
bool barLatched = false;
bool exitingBar = false;
unsigned long exitStart = 0;
const unsigned long EXIT_BAR_TIME = 700;

// Ultrasonic state
bool obstacleDetected = false;
bool lastObstacleSent = false;
unsigned long lastUltrasonicRead = 0;

// PIR state
bool pirDetected = false;
bool lastPirSent = false;

// Serial input
String receivedLine = "";

// PIR scan
const unsigned long scanTime = 3000;

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);

  pinMode(PIR_PIN, INPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  myServo.attach(SERVO_PIN);
  myServo.write(90);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  for (int i = 0; i < 5; i++) {
    pinMode(IR_PINS[i], INPUT);
  }

  stopMotors();

  Serial.println("PIR warming up...");
  delay(30000);
  Serial.println("UNO READY");
}

void loop() {
  readFromESP32();
  checkUltrasonic();

  if (obstacleDetected) {
    stopMotors();
    delay(20);
    return;
  }

  if (robotMode == 'M') {
    applyManualMotion();
    delay(20);
    return;
  }

  if (exitingBar) {
    forwardExit();

    if (millis() - exitStart >= EXIT_BAR_TIME) {
      exitingBar = false;
      Serial.println("EXIT DONE");
    }

    delay(20);
    return;
  }

  runLineFollower();
  delay(20);
}

// ESP32 COMMUNICATION
// format: command,mode,servo,buzzer,speed

void readFromESP32() {
  while (espSerial.available()) {
    char c = espSerial.read();

    if (c == '\n') {
      receivedLine.trim();
      if (receivedLine.length() > 0) {
        parseData(receivedLine);
      }
      receivedLine = "";
    } else {
      receivedLine += c;
    }
  }
}

void parseData(String data) {
  int c1 = data.indexOf(',');
  int c2 = data.indexOf(',', c1 + 1);
  int c3 = data.indexOf(',', c2 + 1);
  int c4 = data.indexOf(',', c3 + 1);

  if (c1 == -1 || c2 == -1 || c3 == -1 || c4 == -1) {
    Serial.println("Invalid format");
    return;
  }

  String cmdStr   = data.substring(0, c1);
  String modeStr  = data.substring(c1 + 1, c2);
  String servoStr = data.substring(c2 + 1, c3);
  String speedStr = data.substring(c4 + 1);

  if (cmdStr.length() > 0) {
    currentCommand = cmdStr.charAt(0);
  }

  if (modeStr.length() > 0) {
    robotMode = modeStr.charAt(0);
  }

  currentServo = servoStr.toInt() ? 1 : 0;
  currentSpeed = constrain(speedStr.toInt(), 0, 255);

  Serial.print("Command: ");
  Serial.println(currentCommand);
  Serial.print("Mode: ");
  Serial.println(robotMode);
  Serial.print("Servo: ");
  Serial.println(currentServo);
  Serial.print("Speed: ");
  Serial.println(currentSpeed);
}

// MANUAL CONTROL

void applyManualMotion() {
  switch (currentCommand) {
    case 'F': forwardManual();  break;
    case 'B': backwardManual(); break;
    case 'L': leftManual();     break;
    case 'R': rightManual();    break;
    case 'S': stopMotors();     break;
    default:  stopMotors();     break;
  }
}

void forwardManual() {
  analogWrite(ENA, currentSpeed);
  analogWrite(ENB, currentSpeed);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void backwardManual() {
  analogWrite(ENA, currentSpeed);
  analogWrite(ENB, currentSpeed);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void leftManual() {
  analogWrite(ENA, currentSpeed);
  analogWrite(ENB, currentSpeed);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void rightManual() {
  analogWrite(ENA, currentSpeed);
  analogWrite(ENB, currentSpeed);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

// AUTO MODE: LINE FOLLOW + STOP BAR

void runLineFollower() {
  readSensors();

  bool stopBar = confirmStopBar();

  if (stopBar && !barLatched) {
    barLatched = true;

    stopMotors();
    Serial.println("STOP BAR DETECTED");

    if (currentServo == 1) {
      Serial.println("RUNNING PIR SCAN");
      runPIRScanSequence();
      Serial.println("PIR SCAN FINISHED");
    } else {
      Serial.println("SERVO DISABLED");
    }

    exitingBar = true;
    exitStart = millis();
    return;
  }

  if (!stopBar) {
    barLatched = false;
  }

  calculateError();

  if (error == 404) {
    stopMotors();
  } else {
    drivePID();
  }
}

// SERVO + PIR SCAN

void runPIRScanSequence() {
  scanPIRAtPosition(180);
  scanPIRAtPosition(0);
  scanPIRAtPosition(90);
  updatePirStatus(false);
}

void scanPIRAtPosition(int angle) {
  myServo.write(angle);
  delay(1000);

  Serial.print("Servo at ");
  Serial.println(angle);
  Serial.println("Scanning...");

  unsigned long startTime = millis();

  while (millis() - startTime < scanTime) {
    bool motion = (digitalRead(PIR_PIN) == HIGH);
    updatePirStatus(motion);

    if (motion) {
      Serial.println("Motion detected");

      while (digitalRead(PIR_PIN) == HIGH) {
        updatePirStatus(true);
        delay(100);
      }

      updatePirStatus(false);
      Serial.println("Motion ended");
    } else {
      Serial.println("No motion");
    }

    delay(500);
  }
}

void updatePirStatus(bool newState) {
  pirDetected = newState;

  if (pirDetected != lastPirSent) {
    if (pirDetected) {
      espSerial.println("PIR:1");
      Serial.println("PIR sent 1");
    } else {
      espSerial.println("PIR:0");
      Serial.println("PIR sent 0");
    }
    lastPirSent = pirDetected;
  }
}

// SENSORS

void readSensors() {
  for (int i = 0; i < 5; i++) {
    sensorValues[i] = digitalRead(IR_PINS[i]);
  }
}

bool isStopBar() {
  int black = 0;
  for (int i = 0; i < 5; i++) {
    if (sensorValues[i] == LINE_COLOUR) black++;
  }
  return (black >= 4);
}

bool confirmStopBar() {
  if (!isStopBar()) return false;

  delay(25);
  readSensors();
  return isStopBar();
}

// PID

void calculateError() {
  int active = 0;
  long avgPos = 0;

  for (int i = 0; i < 5; i++) {
    if (sensorValues[i] == LINE_COLOUR) {
      avgPos += (i - 2);
      active++;
    }
  }

  if (active == 0) {
    error = 404;
  } else {
    error = (float)avgPos / active;
  }
}

void drivePID() {
  integral += error;
  float derivative = error - lastError;
  float turn = (Kp * error) + (Ki * integral) + (Kd * derivative);
  lastError = error;

  int baseSpeed = 85;
  int leftSpeed  = constrain(baseSpeed + turn, 0, 255);
  int rightSpeed = constrain(baseSpeed - turn, 0, 255);

  moveMotors(leftSpeed, rightSpeed);
}

void moveMotors(int left, int right) {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  analogWrite(ENA, left);
  analogWrite(ENB, right);
}

void forwardExit() {
  analogWrite(ENA, 90);
  analogWrite(ENB, 90);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

// ULTRASONIC

void checkUltrasonic() {
  if (millis() - lastUltrasonicRead < 100) return;
  lastUltrasonicRead = millis();

  int distance = readDistanceCm();
  bool newObstacle = (distance > 0 && distance <= STOP_DISTANCE_CM);
  obstacleDetected = newObstacle;

  if (obstacleDetected != lastObstacleSent) {
    if (obstacleDetected) {
      espSerial.println("OBS:1");
      Serial.println("Obstacle detected");
    } else {
      espSerial.println("OBS:0");
      Serial.println("Obstacle cleared");
    }
    lastObstacleSent = obstacleDetected;
  }
}

int readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1;

  return duration * 0.034 / 2;
}

// STOP

void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}