#include <Wire.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <Servo.h>

// Servo motor and sensor setup
Servo servo;
#define SENSOR_PIN A0

// Wi-Fi credentials
#define WIFI_SSID "realme C11"
#define WIFI_PASSWORD "12345678"

// Firebase configuration
#define API_KEY "AIzaSyBYMWrbUh6NpH9xgjJHgfJiiuyzV8EKubk"
#define DATABASE_URL "clothesline-994cf-default-rtdb.firebaseio.com/"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variables for Firebase data
bool signupOK = false;
int autoMode = 0;       // Firebase "Auto" value
int servoValue = 0;     // Firebase "Servo" value
int rainSensorValue = 0; // Analog value from the rain sensor
bool rainDetected = false;
int currentServoState = -1; // Tracks the current state of the servo (-1 = unknown)

// Timing control
unsigned long sendDataPrevMillis = 0;

void setup() {
  // Initialize servo
  servo.attach(D2);
  servo.write(0); // Default position
  currentServoState = 0;

  // Initialize serial and Wi-Fi
  Serial.begin(9600);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Firebase configuration
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase connection successful");
    signupOK = true;
  } else {
    Serial.printf("Firebase sign-up error: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

// Function to read and map the rain sensor value
int readSensor() {
  int sensorValue = analogRead(SENSOR_PIN);  // Read the analog value from sensor
  return map(sensorValue, 0, 1023, 255, 0);  // Map the 10-bit data to 8-bit data
}

void loop() {
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    // Get "Auto" value from Firebase
    if (Firebase.RTDB.getInt(&fbdo, "Auto")) {
      autoMode = fbdo.intData();
      Serial.print("Auto mode: ");
      Serial.println(autoMode);
    } else {
      Serial.println("Failed to read Auto: " + fbdo.errorReason());
    }

    // Get "Servo" value from Firebase
    if (Firebase.RTDB.getInt(&fbdo, "Servo")) {
      servoValue = fbdo.intData();
      Serial.print("Servo value: ");
      Serial.println(servoValue);
    } else {
      Serial.println("Failed to read Servo: " + fbdo.errorReason());
    }

    // Read rain sensor value
    rainSensorValue = readSensor();
    Serial.print("Rain sensor value: ");
    Serial.println(rainSensorValue);

    // Update "Rain" value in Firebase based on rain sensor
    bool currentRainDetected = (rainSensorValue >= 200); // Rain threshold
    if (rainDetected != currentRainDetected) {
      rainDetected = currentRainDetected; // Update only on state change
      if (Firebase.RTDB.setBool(&fbdo, "Rain", rainDetected)) {
        Serial.print("Rain updated to: ");
        Serial.println(rainDetected);
      } else {
        Serial.println("Failed to update Rain: " + fbdo.errorReason());
      }
    }

    // Servo control logic
    if (autoMode == 1) { // Automatic mode
      if (rainDetected && currentServoState == 180) {
        // Rain detected: move servo to 180
        servo.write(0);
        currentServoState = 0;
        Serial.println("Servo automatically set to 180 (rain detected)");
      } else if (!rainDetected && currentServoState == 0) {
        // No rain detected: move servo to 0
        servo.write(180);
        currentServoState = 180;
        Serial.println("Servo automatically set to 0 (no rain detected)");
      }
    } else { // Manual mode
      if (servoValue == 180 && currentServoState != 180) {
        // Manual control to 180
        servo.write(180);
        currentServoState = 180;
        Serial.println("Servo manually set to 180");
      } else if (servoValue == 0 && currentServoState != 0) {
        // Manual control to 0
        servo.write(0);
        currentServoState = 0;
        Serial.println("Servo manually set to 0");
      }
    }

    Serial.println("_______________________________________");
  }
}
