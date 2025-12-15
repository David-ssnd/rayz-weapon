#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "display_init.h"
#include "display_manager.h"
#include "runtime_metrics.h"

#include "debug_print.h"
#include "game_protocol.h"
#include "game_state.h"
#include "gpio_init.h"
#include "tasks.h"
#include "wifi_manager.h"

static const char* TAG = "Weapon";
QueueHandle_t laserMessageQueue;

extern "C" void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(500)); // allow logs to settle

    ESP_LOGI(TAG, "=== RayZ Weapon Starting ===");

    // ---------------------------------------------------------------------
    // Core system init
    // ---------------------------------------------------------------------
    if (!game_state_init(DEVICE_ROLE_WEAPON))
    {
        ESP_LOGE(TAG, "Failed to initialize game state");
        return;
    }

    ESP_LOGI(TAG, "Game state initialized - Device ID: %u", game_state_get_config()->device_id);

    init_reset_button_and_check_factory_reset();
    init_laser_gpio();

    laserMessageQueue = xQueueCreate(5, sizeof(uint32_t));
    if (!laserMessageQueue)
    {
        ESP_LOGE(TAG, "Failed to create laser message queue");
        return;
    }

    // ---------------------------------------------------------------------
    // Display hardware init (LVGL + SSD1306)
    // ---------------------------------------------------------------------
    lv_disp_t* disp = init_display();
    if (!disp)
    {
        ESP_LOGW(TAG, "Display init failed, continuing headless");
    }
    else
    {
        // -----------------------------------------------------------------
        // Display manager init (UI + state machine)
        // -----------------------------------------------------------------
        dm_sources_t dm_sources = {
            .wifi_connected = wifi_manager_is_connected,
            .wifi_ip = wifi_manager_get_ip,
            .wifi_rssi = wifi_manager_get_rssi,

            .uptime_ms = system_uptime_ms,
            .free_heap = system_free_heap,

            .player_id = metric_player_id,
            .device_id = metric_device_id,
            .ammo = metric_ammo,
            .last_rx_ms_ago = metric_last_rx_ms_ago,
            .rx_count = metric_rx_count,
            .tx_count = metric_tx_count,
        };

        if (!display_manager_init(disp, &dm_sources))
        {
            ESP_LOGW(TAG, "Display manager init failed");
        }
        else
        {
            xTaskCreate(display_manager_task, "display_manager", 4096, NULL, 3, NULL);
        }
    }

    debug_print_nvs_contents();

    // ---------------------------------------------------------------------
    // Tasks
    // ---------------------------------------------------------------------
    xTaskCreate(control_task, "control", 4096, NULL, 5, NULL);
    xTaskCreate(laser_task, "laser", 2048, NULL, 4, NULL);
    xTaskCreate(game_task, "game", 4096, NULL, 2, NULL);
    xTaskCreate(espnow_task, "espnow", 4096, NULL, 3, NULL);
    xTaskCreate(wifi_task, "wifi", 4096, NULL, 1, NULL);
    xTaskCreate(ws_task, "websocket", 8192, NULL, 2, NULL);

    ESP_LOGI(TAG, "All tasks created successfully");
}
