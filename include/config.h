#ifndef CONFIG_H
#define CONFIG_H

#include "protocol_config.h"

#define LASER_PIN 5
#define RESET_BUTTON_PIN 0    // GPIO 0 with pull-up
#define TRIGGER_BUTTON_PIN 10 // TODO: Set correct GPIO for trigger button

#define RESET_DELAY_MS 2000

// I2C pins for OLED display
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define OLED_I2C_ADDR 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 32

#endif
