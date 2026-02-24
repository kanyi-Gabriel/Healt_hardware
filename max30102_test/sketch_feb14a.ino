#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include "MAX30105.h"

// --- 1. CONFIGURATION ---
const char* ssid = "Fayouu";
const char* password = "Indomitable";

// Replace 192.168.X.X with your computer's actual local IP address!
const char* serverUrl = "http://1192.168.100.23:8000/hscan/upload-vitals/"; 

MAX30105 particleSensor;

// --- 2. BATCHING VARIABLES ---
const int BATCH_SIZE = 100;
long irBatch[BATCH_SIZE];
long redBatch[BATCH_SIZE];
int sampleCount = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);

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
      Serial.print(WiFi.RSSI(i));  // Signal strength in dBm; > -70 is good
      Serial.print(" dBm) ");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Encrypted");
    }
  }
  Serial.println("");

  // Now proceed to connect
  WiFi.begin(ssid, password);
  // ... (rest of your setup)
}
void loop() {
  long irValue = particleSensor.getIR();
  long redValue = particleSensor.getRed();

  // Check if a finger is actually on the sensor (IR value spikes when a finger is present)
  if (irValue > 50000) {
    // Store the reading in our arrays
    irBatch[sampleCount] = irValue;
    redBatch[sampleCount] = redValue;
    sampleCount++;

    // Print progress to the Serial Monitor
    if (sampleCount % 10 == 0) {
      Serial.print("Collecting batch... ");
      Serial.print(sampleCount);
      Serial.println("%");
    }

    // Once we hit 100 samples, send the payload to Django
    if (sampleCount >= BATCH_SIZE) {
      sendDataToDjango();
      sampleCount = 0; // Reset counter for the next batch
      
      Serial.println("Remove finger to reset, or keep it on for next batch.");
      delay(3000); // Wait 3 seconds before taking another reading
    }
    
    delay(10); // ~100Hz sample rate (10ms between readings)
    
  } else {
    // If the finger is removed halfway through, discard the incomplete batch
    if (sampleCount > 0) {
      Serial.println("Finger removed. Discarding incomplete batch.");
      sampleCount = 0;
    }
    delay(100); // Idle delay
  }
}

void sendDataToDjango() {
  // 1. SAFETY NET: If Wi-Fi dropped during the 1-second data collection, force it back on
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

  // 2. Execute the HTTP POST only if the connection is secured
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Preparing JSON payload...");

    // Pre-allocate memory to prevent RAM crashing
    String jsonPayload;
    jsonPayload.reserve(3000); 

    // Build the string
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
    jsonPayload += "]}";

    Serial.println("Sending HTTP POST request to Auratrack backend...");

    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    // Send the safely constructed payload
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