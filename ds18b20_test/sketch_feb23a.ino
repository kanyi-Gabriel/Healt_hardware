#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is connected to digital pin D2
#define ONE_WIRE_BUS 4

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to the Dallas Temperature library
DallasTemperature sensors(&oneWire);

void setup(void) {
  // Start serial communication for debugging
  Serial.begin(115200);
  Serial.println("DS18B20 Temperature Sensor Test");
  
  // Start up the library
  sensors.begin();
}

void loop(void) {
  // Send the command to get temperatures
  sensors.requestTemperatures(); 
  
  // Read the temperature in Celsius from the first sensor (index 0)
  float tempC = sensors.getTempCByIndex(0);

  // Check if the reading was successful
  if(tempC != DEVICE_DISCONNECTED_C) {
    Serial.print("Temperature: ");
    Serial.print(tempC);
    Serial.println(" °C");
  } else {
    Serial.println("Error: Could not read temperature data. Check wiring!");
  }
  
  // Wait 1 second before the next reading
  delay(1000);
}