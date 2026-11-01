# AI-Powered Health Monitoring System — Hardware
Firmware for the sensing side of an AI-powered health monitoring system. An ESP32 reads a person's pulse signal from a MAX30102 pulse-oximetry sensor and their body temperature from a DS18B20 sensor, then sends the readings over Wi-Fi to a Django REST API. The Django side calculates heart rate and SpO₂ and uses Google Gemini to explain the results in plain language.
## Table of contents
- How it works
- Hardware components
- Wiring
- Repository structure
- Setup and installation
- Data sent to the server
- Testing
- Troubleshooting
- Known limitations and next steps

