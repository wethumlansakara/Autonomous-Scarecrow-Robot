#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include "ExampleFunctions.h"
#include <ArduinoJson.h>

// ================= WIFI =================
#define WIFI_SSID "HONOR X6b"
#define WIFI_PASSWORD "12345677"

// ================= FIREBASE =================
#define Web_API_KEY "AIzaSyBaNpj3dAiUo4LvqWuhFgN1Mf_Ot_ME9YI"
#define DATABASE_URL "https://autonomous-scarecrow-robot-default-rtdb.firebaseio.com/"
#define USER_EMAIL "wethumlansakara2007@gmail.com"
#define USER_PASS "WethumKL20071029"

// ================= BUZZER =================
#define BUZZER_PIN 13

// ================= ACTUATOR =================
#define ACT_IN1 26
#define ACT_IN2 27
#define ACT_EN  25
#define ACT_SPEED 100

void processData(AsyncResult &aResult);
void sendToUno();
void readFromUno();
void updateBuzzer();
void updateFirebaseStates();
void actuatorON();
void actuatorOFF();

UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);

SSL_CLIENT ssl_client, stream_ssl_client;
FirebaseApp app;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client), streamClient(stream_ssl_client);
RealtimeDatabase Database;

String listenerPath = "/robot";

// Firebase-controlled values
char currentCommand = 'S';
char robotMode = 'A';
int currentServo = 1;
int currentBuzzer = 0;
int currentSpeed = 90;
int firebaseActuator = 0;

// sensor states from UNO
bool obstacleDetected = false;
bool pirDetected = false;

// actuator
bool actuatorState = false;

// upload cache
int lastUltrasonicValue = -1;
int lastPirValue = -1;
int lastActuatorValue = -1;

String unoLine = "";

void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println();
  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(ACT_IN1, OUTPUT);
  pinMode(ACT_IN2, OUTPUT);
  ledcAttach(ACT_EN, 1000, 8);
  actuatorOFF();

  Serial2.begin(9600, SERIAL_8N1, 16, 17); //RX TX

  initWiFi();

  ssl_client.setInsecure();
  stream_ssl_client.setInsecure();

  ssl_client.setConnectionTimeout(1000);
  ssl_client.setHandshakeTimeout(5);
  stream_ssl_client.setConnectionTimeout(1000);
  stream_ssl_client.setHandshakeTimeout(5);

  initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");

  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);

  streamClient.setSSEFilters("get,put,patch,keep-alive,cancel,auth_revoked");
  Database.get(streamClient, listenerPath, processData, true, "streamTask");

  Serial.println("ESP32 READY");
}

void loop() {
  app.loop();
  readFromUno();
  updateBuzzer();

  if (pirDetected) {
  actuatorON();
  } else {
  actuatorOFF();
  }

  updateFirebaseStates();
}

void processData(AsyncResult &aResult) {
  if (!aResult.isResult()) return;

  if (aResult.isError()) {
    Firebase.printf("Error task: %s, msg: %s, code: %d\n",
                    aResult.uid().c_str(),
                    aResult.error().message().c_str(),
                    aResult.error().code());
  }

  if (!aResult.available()) return;

  RealtimeDatabaseResult &RTDB = aResult.to<RealtimeDatabaseResult>();
  if (!RTDB.isStream()) return;

  String path = RTDB.dataPath();

  if (RTDB.type() == 6) {
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, RTDB.to<String>());

    if (!error) {
      if (doc["command"].is<const char*>()) {
        currentCommand = doc["command"].as<const char*>()[0];
      }

      if (doc["mode"].is<const char*>()) {
        robotMode = doc["mode"].as<const char*>()[0];
      }

      if (doc["servo"].is<int>()) {
        currentServo = doc["servo"].as<int>();
      } else if (doc["servo"].is<bool>()) {
        currentServo = doc["servo"].as<bool>() ? 1 : 0;
      }

      if (doc["buzzer"].is<int>()) {
        currentBuzzer = doc["buzzer"].as<int>();
      } else if (doc["buzzer"].is<bool>()) {
        currentBuzzer = doc["buzzer"].as<bool>() ? 1 : 0;
      }

      if (doc["speed"].is<int>()) {
        currentSpeed = doc["speed"].as<int>();
      }

      if (doc["actuator"].is<int>()) {
        firebaseActuator = doc["actuator"].as<int>();
      } else if (doc["actuator"].is<bool>()) {
        firebaseActuator = doc["actuator"].as<bool>() ? 1 : 0;
      }

      currentServo = currentServo ? 1 : 0;
      currentBuzzer = currentBuzzer ? 1 : 0;
      firebaseActuator = firebaseActuator ? 1 : 0;
      currentSpeed = constrain(currentSpeed, 0, 255);

      sendToUno();
    }
  }
  else if (path == "/command") {
    String cmd = RTDB.to<String>();
    cmd.replace("\"", "");
    if (cmd.length() > 0) currentCommand = cmd.charAt(0);
    sendToUno();
  }
  else if (path == "/mode") {
    String mode = RTDB.to<String>();
    mode.replace("\"", "");
    if (mode.length() > 0) robotMode = mode.charAt(0);
    sendToUno();
  }
  else if (path == "/servo") {
    if (RTDB.type() == 1) currentServo = RTDB.to<int>();
    else if (RTDB.type() == 4) currentServo = RTDB.to<bool>() ? 1 : 0;
    currentServo = currentServo ? 1 : 0;
    sendToUno();
  }
  else if (path == "/buzzer") {
    if (RTDB.type() == 1) currentBuzzer = RTDB.to<int>();
    else if (RTDB.type() == 4) currentBuzzer = RTDB.to<bool>() ? 1 : 0;
    currentBuzzer = currentBuzzer ? 1 : 0;
    updateBuzzer();
    sendToUno();
  }
  else if (path == "/speed") {
    if (RTDB.type() == 1) currentSpeed = RTDB.to<int>();
    currentSpeed = constrain(currentSpeed, 0, 255);
    sendToUno();
  }
  else if (path == "/actuator") {
    if (RTDB.type() == 1) {
      firebaseActuator = RTDB.to<int>();
    } else if (RTDB.type() == 4) {
      firebaseActuator = RTDB.to<bool>() ? 1 : 0;
    }
    firebaseActuator = firebaseActuator ? 1 : 0;
  }
}

void sendToUno() {
  char sendCmd = obstacleDetected ? 'S' : currentCommand;

  String out = String(sendCmd) + "," +
               String(robotMode) + "," +
               String(currentServo) + "," +
               String(currentBuzzer) + "," +
               String(currentSpeed);

  Serial.print("Send to Uno: ");
  Serial.println(out);

  Serial2.print(out);
  Serial2.print('\n');
}

void readFromUno() {
  while (Serial2.available()) {
    char c = Serial2.read();

    if (c == '\n') {
      unoLine.trim();

      if (unoLine.length() > 0) {
        Serial.print("From Uno: ");
        Serial.println(unoLine);

        if (unoLine == "OBS:1") {
          obstacleDetected = true;
          sendToUno();
          updateBuzzer();
        }
        else if (unoLine == "OBS:0") {
          obstacleDetected = false;
          sendToUno();
          updateBuzzer();
        }
        else if (unoLine == "PIR:1") {
          pirDetected = true;
          updateBuzzer();
        }
        else if (unoLine == "PIR:0") {
          pirDetected = false;
          updateBuzzer();
        }
      }

      unoLine = "";
    } else {
      unoLine += c;
    }
  }
}

void updateBuzzer() {
  if (currentBuzzer == 1 || obstacleDetected || pirDetected) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void actuatorON() {
  digitalWrite(ACT_IN1, HIGH);
  digitalWrite(ACT_IN2, LOW);
  ledcWrite(ACT_EN, ACT_SPEED);
  actuatorState = true;
}

void actuatorOFF() {
  ledcWrite(ACT_EN, 0);
  digitalWrite(ACT_IN1, LOW);
  digitalWrite(ACT_IN2, LOW);
  actuatorState = false;
}

void updateFirebaseStates() {
  if (!app.ready()) return;

  int ultrasonicValue = obstacleDetected ? 1 : 0;
  int pirValue = pirDetected ? 1 : 0;
  int actuatorValue = actuatorState ? 1 : 0;

  if (ultrasonicValue != lastUltrasonicValue) {
    Database.set<int>(aClient, "/robot/ultrasonic", ultrasonicValue, processData, "setUltrasonic");
    lastUltrasonicValue = ultrasonicValue;
  }

  if (pirValue != lastPirValue) {
    Database.set<int>(aClient, "/robot/pir", pirValue, processData, "setPir");
    lastPirValue = pirValue;
  }

  if (actuatorValue != lastActuatorValue) {
    Database.set<int>(aClient, "/robot/actuator", actuatorValue, processData, "setActuatorState");
    lastActuatorValue = actuatorValue;
  }
}