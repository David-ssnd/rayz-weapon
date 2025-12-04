#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "ble_weapon.h"
#include "tasks.h"

static const char* TAG = "BLETask";

extern BLEWeapon bleWeapon;

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
