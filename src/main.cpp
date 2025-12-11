// this line is here in order to not break the order of the includes because freertos must be before events
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <esp_lcd_panel_ops.h>
#include <esp_log.h>
#include <lvgl.h>
#include "ble_weapon.h"
#include "game_protocol.h"
#include "game_state.h"
#include "wifi_manager.h"


// Task includes
#include "tasks.h"

// Hardware initialization includes
#include "display_init.h"
#include "gpio_init.h"

static const char* TAG = "Weapon";

// Global objects
BLEWeapon bleWeapon;
QueueHandle_t laserMessageQueue;

// Display state
esp_lcd_panel_handle_t panel_handle = NULL;
bool display_initialized = false;
uint16_t g_message_count = 0;

// LVGL objects
lv_disp_t* lvgl_display = NULL;
lv_obj_t* label_wifi = NULL;

// WiFi initialization task - runs independently to not block critical functions
void wifi_init_task(void* pvParameters)
{
    ESP_LOGI(TAG, "WiFi initialization task started");
    wifi_manager_init("rayz-weapon", "weapon");
    // Task completes after WiFi init
    vTaskDelete(NULL);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== RayZ Weapon Starting ===");

    // Initialize game state manager first
    if (!game_state_init(DEVICE_ROLE_WEAPON))
    {
        ESP_LOGE(TAG, "Failed to initialize game state");
        return;
    }
    ESP_LOGI(TAG, "Game state initialized - Device ID: %s", game_state_get_config()->device_id);

    // Check for factory reset
    init_reset_button_and_check_factory_reset();

    // Initialize laser GPIO
    init_laser_gpio();

    // Create message queue
    laserMessageQueue = xQueueCreate(5, sizeof(uint16_t));
    if (laserMessageQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create laser message queue");
        return;
    }

    // Initialize display
    if (!init_display())
    {
        ESP_LOGW(TAG, "Display initialization failed, continuing without display");
    }

    // Create FreeRTOS tasks - critical tasks first
    xTaskCreate(control_task, "control", 4096, NULL, 5, NULL);
    xTaskCreate(laser_task, "laser", 2048, NULL, 4, NULL);
    xTaskCreate(ble_task, "ble", 8192, NULL, 3, NULL);
    xTaskCreate(display_task, "display", 4096, NULL, 2, NULL);
    xTaskCreate(game_task, "game", 4096, NULL, 2, NULL);

    // WiFi init in separate low-priority task to not block critical functions
    xTaskCreate(wifi_init_task, "wifi_init", 4096, NULL, 1, NULL);

    // WebSocket task starts after WiFi is ready (it waits internally)
    xTaskCreate(ws_task, "websocket", 8192, NULL, 2, NULL);

    ESP_LOGI(TAG, "All tasks created successfully");
}
