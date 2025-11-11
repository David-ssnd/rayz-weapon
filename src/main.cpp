#include <Arduino.h>
#include "config.h"
#include "hash.h"
#include "utils.h"
#include "ble_weapon.h"

BLEWeapon bleWeapon;

void setup() {
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW);
  
  Serial.begin(115200);
  delay(1000);
  
  bleWeapon.begin();
}

void sendBit(bool bit) {
  digitalWrite(LASER_PIN, bit ? HIGH : LOW);
  delay(BIT_DURATION_MS);
}

void sendMessage(uint16_t message) {
  for (int i = MESSAGE_TOTAL_BITS - 1; i >= 0; i--) {
    bool bit = (message >> i) & 0x01;
    sendBit(bit);
  }
  digitalWrite(LASER_PIN, LOW);
}

uint16_t data = 0;

void loop() {
  data++;

  uint16_t message = createMessage16bit(data);
  
  // Send via BLE
  if (bleWeapon.isConnected()) {
    bleWeapon.sendMessage(message);
    Serial.print("📡 BLE sent | ");
  } else {
    Serial.print("⚠️  BLE N/A | ");
  }
  
  // Send via laser
  sendMessage(message);
  
  Serial.print("► Laser | ");
  Serial.print(millis());
  Serial.print(" ms | ");
  Serial.print(toBinaryString(message, MESSAGE_TOTAL_BITS));
  Serial.print(" | Data: ");
  Serial.println(data);

  bleWeapon.handleConnection();

  delay(TRANSMISSION_PAUSE_MS);
}
