#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// WiFi credentials
#define WIFI_SSID "HONOR X6b"
#define WIFI_PASSWORD "12345677"

// Firebase credentials
#define API_KEY "AIzaSyBaNpj3dAiUo4LvqWuhFgN1Mf_Ot_ME9YI"
#define DATABASE_URL "https://autonomous-scarecrow-robot-default-rtdb.firebaseio.com"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Command variable
String lastCommand = "";

void setup() {
  Serial.begin(9600);   // UART to Arduino Uno
  Serial.println();

  // Connect WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("\nConnected!");
  Serial.println(WiFi.localIP());

  // Firebase config
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase connected");
}

void loop() {

  // Read command from Firebase
  if (Firebase.RTDB.getString(&fbdo, "/robot/command")) {
    
    String command = fbdo.stringData();

    // Only send if changed
    if (command != lastCommand) {
      Serial.print("New Command: ");
      Serial.println(command);

      sendToArduino(command);
      lastCommand = command;
    }

  } else {
    Serial.print("Firebase Error: ");
    Serial.println(fbdo.errorReason());
  }

  delay(200); // small delay (avoid flooding)
}

// Send command to Arduino
void sendToArduino(String cmd) {
  if (cmd == "F") {
    Serial.write('F');
  } 
  else if (cmd == "B") {
    Serial.write('B');
  }
  else if (cmd == "L") {
    Serial.write('L');
  }
  else if (cmd == "R") {
    Serial.write('R');
  }
  else if (cmd == "S") {
    Serial.write('S');
  }
}