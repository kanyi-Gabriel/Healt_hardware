#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include "MAX30105.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// --- 1. CONFIGURATION ---
const char* ssid = "Fayouu";
const char* password = "Indomitable";

// Replace 192.168.X.X with your computer's actual local IP address!
const char* serverUrl = "http://192.168.100.23:8000/hscan/upload-vitals/";  

// DS18B20 Configuration
#define ONE_WIRE_BUS 4  // GPIO4 (D2) - Change if using different pin
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

MAX30105 particleSensor;

// --- 2. BATCHING VARIABLES ---
const int BATCH_SIZE = 100;
long irBatch[BATCH_SIZE];
long redBatch[BATCH_SIZE];
int sampleCount = 0;

// Temperature reading (take average of multiple readings)
float temperature = 0.0;
const int TEMP_READINGS = 5;

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Initialize DS18B20
  sensors.begin();
  Serial.println("DS18B20 Temperature Sensor initialized");

  // Scan for networks
  Serial.println("Scanning for WiFi networks...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  Serial.println("Scan done!");
  if (n == 0) {
    Serial.println("No networks found. Check ESP32 power/antenna.");
  } else {
    Serial.print(n);
    Serial.println(" networks found:");
    for (int i = 0; i < n; ++i) {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(" dBm) ");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Encrypted");
    }
  }
  Serial.println("");

  // Connect to WiFi
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  Serial.print("Connecting to WiFi");
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi!");
    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect. Status: " + String(WiFi.status()));
  }

  // Initialize MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not found. Please check wiring.");
    while (1);
  }
  particleSensor.setup();
  
  Serial.println("System ready. Place finger on sensor and ensure temperature sensor is connected...");
}

void loop() {
  long irValue = particleSensor.getIR();
  long redValue = particleSensor.getRed();

  // Check if a finger is on the sensor (IR value spikes when finger is present)
  if (irValue > 50000) {
    // Store the reading in our arrays
    irBatch[sampleCount] = irValue;
    redBatch[sampleCount] = redValue;
    sampleCount++;

    // Print progress
    if (sampleCount % 10 == 0) {
      Serial.print("Collecting batch... ");
      Serial.print(sampleCount);
      Serial.println("%");
    }

    // Once we hit 100 samples, send the payload to Django
    if (sampleCount >= BATCH_SIZE) {
      // Get temperature reading
      temperature = readTemperature();
      Serial.print("Temperature reading: ");
      Serial.println(temperature);
      
      sendDataToDjango();
      sampleCount = 0;
      
      Serial.println("Remove finger to reset, or keep it on for next batch.");
      delay(3000);
    }
    
    delay(10); // ~100Hz sample rate
    
  } else {
    if (sampleCount > 0) {
      Serial.println("Finger removed. Discarding incomplete batch.");
      sampleCount = 0;
    }
    delay(100);
  }
}

float readTemperature() {
  float tempSum = 0.0;
  int validReadings = 0;
  
  // Take multiple readings and average them
  for (int i = 0; i < TEMP_READINGS; i++) {
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);
    
    // Check for valid reading (DS18B20 returns -127 for error)
    if (tempC != -127.00 && tempC != 85.00) {
      tempSum += tempC;
      validReadings++;
    }
    delay(100);
  }
  
  if (validReadings > 0) {
    return tempSum / validReadings;
  } else {
    return 0.0; // Return 0 if sensor error
  }
}

void sendDataToDjango() {
  // WiFi reconnection if dropped
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi dropped during collection! Forcing reconnect...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Preparing JSON payload...");

    String jsonPayload;
    jsonPayload.reserve(3500);

    // Build JSON with IR, Red batches AND temperature
    jsonPayload = "{\"ir_batch\":[";
    for (int i = 0; i < BATCH_SIZE; i++) {
      jsonPayload += String(irBatch[i]);
      if (i < BATCH_SIZE - 1) jsonPayload += ",";
    }
    jsonPayload += "],\"red_batch\":[";
    for (int i = 0; i < BATCH_SIZE; i++) {
      jsonPayload += String(redBatch[i]);
      if (i < BATCH_SIZE - 1) jsonPayload += ",";
    }
    jsonPayload += "],\"temperature\":" + String(temperature) + "}";

    Serial.println("Sending HTTP POST request to Auratrack backend...");

    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response Code: ");
      Serial.println(httpResponseCode);
      
      String responseString = http.getString();
      Serial.println("Django Response: " + responseString);
    } else {
      Serial.print("Error sending POST request. Code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("Error: WiFi completely dead. Batch dropped. Check power supply.");
  }
}
