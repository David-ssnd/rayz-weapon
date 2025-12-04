#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "ble_weapon.h"
#include "hash.h"
#include "protocol_config.h"
#include "tasks.h"
#include "utils.h"
#include "wifi_manager.h"

static const char* TAG = "ControlTask";

extern BLEWeapon bleWeapon;
extern QueueHandle_t laserMessageQueue;
extern uint16_t g_message_count;

void control_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Control task started");
    uint16_t data = 0;

    while (1)
    {
        if (!wifi_manager_is_connected())
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        data++;
        g_message_count++;
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
            ESP_LOGI(TAG, "[BLE+Laser] %lu ms | %s | Data: %u", pdTICKS_TO_MS(xTaskGetTickCount()),
                     toBinaryString(message, MESSAGE_TOTAL_BITS).c_str(), data);
        }
        else
        {
            ESP_LOGI(TAG, "[Laser only] %lu ms | %s | Data: %u", pdTICKS_TO_MS(xTaskGetTickCount()),
                     toBinaryString(message, MESSAGE_TOTAL_BITS).c_str(), data);
        }

        vTaskDelay(pdMS_TO_TICKS(TRANSMISSION_PAUSE_MS));
    }
}
