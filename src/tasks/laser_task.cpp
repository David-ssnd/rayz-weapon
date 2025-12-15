#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include "config.h"
#include "protocol_config.h"
#include "tasks.h"

static const char* TAG = "LaserTask";

extern QueueHandle_t laserMessageQueue;

static void sendMessage(uint32_t message)
{
    TickType_t nextWake = xTaskGetTickCount();
    const TickType_t bitPeriod = pdMS_TO_TICKS(BIT_DURATION_MS);

    for (int i = MESSAGE_TOTAL_BITS - 1; i >= 0; i--)
    {
        bool bit = (message >> i) & 0x01;
        gpio_set_level((gpio_num_t)LASER_PIN, bit ? 1 : 0);
        vTaskDelayUntil(&nextWake, bitPeriod);
    }
    gpio_set_level((gpio_num_t)LASER_PIN, 0);
}

void laser_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Laser task started");
    uint32_t message;

    while (1)
    {
        if (xQueueReceive(laserMessageQueue, &message, portMAX_DELAY) == pdTRUE)
        {
            sendMessage(message);
        }
    }
}
