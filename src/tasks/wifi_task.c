#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "tasks.h"
#include "wifi_manager.h"

static const char* TAG = "WiFiTask";

void wifi_task(void* pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "WiFi initialization task started");
    wifi_manager_init("rayz-weapon", "weapon");
    vTaskDelete(NULL);
}
