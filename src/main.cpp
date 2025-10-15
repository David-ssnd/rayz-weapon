// Simple test for RayZ weapon
// Prints a message to Serial and blinks GPIO 2 (common on ESP32 dev boards)

#include <Arduino.h>

const int LED_PIN = 2; // fallback to GPIO2

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Serial.println("RayZ weapon: Hello from ESP32!");
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  delay(200);
  digitalWrite(LED_PIN, LOW);
  delay(800);
  Serial.println("ping");
}
