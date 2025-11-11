#include "ble_weapon.h"
#include "config.h"
#include "hash.h"
#include "utils.h"
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>


static const char* TAG = "Weapon";

BLEWeapon bleWeapon;

// FreeRTOS queues
QueueHandle_t laserMessageQueue;

void sendBit(bool bit)
{
    gpio_set_level((gpio_num_t)LASER_PIN, bit ? 1 : 0);
    vTaskDelay(pdMS_TO_TICKS(BIT_DURATION_MS));
}

void sendMessage(uint16_t message)
{
    for (int i = MESSAGE_TOTAL_BITS - 1; i >= 0; i--)
    {
        bool bit = (message >> i) & 0x01;
        sendBit(bit);
    }
    gpio_set_level((gpio_num_t)LASER_PIN, 0);
}

void control_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Control task started");
    uint16_t data = 0;

    while (1)
    {
        data++;
        uint16_t message = createMessage16bit(data);

        // Send to laser queue
        if (xQueueSend(laserMessageQueue, &message, 0) != pdTRUE)
        {
            ESP_LOGW(TAG, "Failed to send to laser queue");
        }

        // Send via BLE
        if (bleWeapon.isConnected())
        {
            bleWeapon.sendMessage(message);
            ESP_LOGI(TAG, "📡 BLE sent | ► Laser | %lu ms | %s | Data: %u",
                     pdTICKS_TO_MS(xTaskGetTickCount()),
                     toBinaryString(message, MESSAGE_TOTAL_BITS).c_str(), data);
        }
        else
        {
            ESP_LOGI(TAG, "⚠️  BLE N/A | ► Laser | %lu ms | %s | Data: %u",
                     pdTICKS_TO_MS(xTaskGetTickCount()),
                     toBinaryString(message, MESSAGE_TOTAL_BITS).c_str(), data);
        }

        vTaskDelay(pdMS_TO_TICKS(TRANSMISSION_PAUSE_MS));
    }
}

void laser_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Laser task started");
    uint16_t message;

    while (1)
    {
        if (xQueueReceive(laserMessageQueue, &message, portMAX_DELAY) == pdTRUE)
        {
            sendMessage(message);
        }
    }
}

void ble_task(void* pvParameters)
{
    ESP_LOGI(TAG, "BLE task started");

    bleWeapon.begin();

    while (1)
    {
        bleWeapon.handleConnection();
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Weapon device starting...");

    // Initialize GPIO for laser
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LASER_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)LASER_PIN, 0);

    // Create queues
    laserMessageQueue = xQueueCreate(5, sizeof(uint16_t));

    if (laserMessageQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create laser queue");
        return;
    }

    ESP_LOGI(TAG, "Weapon device ready");

    // Create tasks
    xTaskCreate(control_task, "control", 4096, NULL, 5, NULL);
    xTaskCreate(laser_task, "laser", 2048, NULL, 4, NULL);
    xTaskCreate(ble_task, "ble", 8192, NULL, 3, NULL);
}
