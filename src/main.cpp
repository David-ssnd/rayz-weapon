#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "config.h"
#include "debug_print.h"
#include "display_init.h"
#include "display_manager.h"
#include "game_protocol.h"
#include "game_state.h"
#include "gpio_init.h"
#include "runtime_metrics.h"
#include "tasks.h"
#include "wifi_manager.h"
#include "ws_server.h"

static const char* TAG = "Weapon";
QueueHandle_t laserMessageQueue;

static bool is_ws_connected(void)
{
    return ws_server_client_count() > 0;
}

extern "C" void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_LOGI(TAG, "=== RayZ Weapon Starting ===");

    if (!game_state_init(DEVICE_ROLE_WEAPON))
    {
        ESP_LOGE(TAG, "Failed to initialize game state");
        return;
    }
    ESP_LOGI(TAG, "Game state initialized - Device ID: %u", game_state_get_config()->device_id);

    init_reset_button_and_check_factory_reset();
    init_laser_gpio(LASER_PIN);

    laserMessageQueue = xQueueCreate(5, sizeof(uint32_t));
    if (!laserMessageQueue)
    {
        ESP_LOGE(TAG, "Failed to create laser message queue");
        return;
    }

    lv_disp_t* disp = init_display();
    if (!disp)
    {
        ESP_LOGW(TAG, "Display init failed, continuing headless");
    }
    else
    {
        dm_sources_t dm_sources = {
            .wifi_connected = wifi_manager_is_connected,
            .wifi_ip = wifi_manager_get_ip,
            .wifi_ssid = wifi_manager_get_ssid,
            .wifi_status = wifi_manager_get_status_string,
            .wifi_rssi = wifi_manager_get_rssi,
            .uptime_ms = system_uptime_ms,
            .free_heap = system_free_heap,
            .ws_connected = is_ws_connected,
            .device_name = wifi_manager_get_device_name,
            .player_id = metric_player_id,
            .device_id = metric_device_id,
            .ammo = metric_ammo,
            .last_rx_ms_ago = metric_last_rx_ms_ago,
            .rx_count = metric_rx_count,
            .tx_count = metric_tx_count,
            .hit_count = nullptr,
            .last_hit_ms_ago = nullptr,
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

    ESP_LOGI(TAG, "Weapon device ready");
    xTaskCreate(control_task, "control", 4096, NULL, 5, NULL);
    xTaskCreate(laser_task, "laser", 2048, NULL, 4, NULL);
    xTaskCreate(game_task, "game", 4096, NULL, 2, NULL);
    xTaskCreate(espnow_task, "espnow", 4096, NULL, 3, NULL);
    xTaskCreate(wifi_task, "wifi", 4096, NULL, 1, NULL);
    xTaskCreate(ws_task, "websocket", 8192, NULL, 2, NULL);
    ESP_LOGI(TAG, "All tasks created");
}
