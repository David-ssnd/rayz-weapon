// RayZ Weapon Device
// Using shared library for common protocol definitions

#include <Arduino.h>
#include "rayz_common.h"

const int LED_PIN = 2; // fallback to GPIO2

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  
  Serial.println("=== RayZ Weapon Device ===");
  Serial.printf("Protocol Version: %s\n", RAYZ_PROTOCOL_VERSION);
  Serial.printf("Build Version: %s\n", RAYZ_VERSION);
  Serial.printf("Device Type: WEAPON (0x%02X)\n", DEVICE_WEAPON);
  Serial.println("=========================");
  
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  // Create a heartbeat message using shared protocol
  RayZMessage msg;
  msg.deviceType = DEVICE_WEAPON;
  msg.messageType = MSG_HEARTBEAT;
  msg.payloadLength = 0;
  
  digitalWrite(LED_PIN, HIGH);
  delay(200);
  digitalWrite(LED_PIN, LOW);
  delay(800);
  
  Serial.printf("Weapon heartbeat - Type: 0x%02X, Msg: 0x%02X\n", 
                msg.deviceType, msg.messageType);
}
