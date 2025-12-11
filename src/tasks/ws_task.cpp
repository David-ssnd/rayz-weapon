#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "game_state.h"
#include "tasks.h"
#include "wifi_manager.h"
#include "ws_client.h"


static const char* TAG = "WsTask";

// WebSocket server URI - configure this for your server
#define WS_SERVER_URI "ws://192.168.1.100:3000/ws"

// Callbacks
static void on_ws_connect(bool connected)
{
    ESP_LOGI(TAG, "WebSocket %s", connected ? "connected" : "disconnected");
}

static void on_ws_hit(const char* shooter_id, const char* target_id, bool valid)
{
    ESP_LOGI(TAG, "Hit event: %s -> %s (%s)", shooter_id, target_id, valid ? "valid" : "invalid");

    // If we're the shooter and it's valid, we got a kill
    const DeviceConfig* config = game_state_get_config();
    if (valid && strcmp(config->player_id, shooter_id) == 0)
    {
        game_state_record_kill();
    }
}

static void on_ws_game_state(GameMode mode, GameState state)
{
    ESP_LOGI(TAG, "Game state: mode=%s, state=%s", GAMEMODE_NAMES[mode], GAME_STATE_NAMES[state]);
}

static void on_ws_config(const char* json)
{
    ESP_LOGI(TAG, "Config update received");
    // Config is already applied by ws_client
}

void ws_task(void* pvParameters)
{
    ESP_LOGI(TAG, "WebSocket task started");

    // Wait for WiFi connection
    while (!wifi_manager_is_connected())
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI(TAG, "WiFi connected, initializing WebSocket client");

    // Generate default device name if not set
    DeviceConfig* config = game_state_get_config_mut();
    if (strlen(config->device_name) == 0)
    {
        const char* ip_str = wifi_manager_get_ip();
        game_state_generate_name(config->device_name, sizeof(config->device_name), config->role, ip_str);
        game_state_save_config();
        ESP_LOGI(TAG, "Generated device name: %s", config->device_name);
    }

    // Initialize WebSocket client
    WsClientConfig ws_config = {.server_uri = WS_SERVER_URI,
                                .on_connect = on_ws_connect,
                                .on_message = NULL, // Use specific callbacks instead
                                .on_hit = on_ws_hit,
                                .on_game_state = on_ws_game_state,
                                .on_config = on_ws_config};

    if (!ws_client_init(&ws_config))
    {
        ESP_LOGE(TAG, "Failed to initialize WebSocket client");
        vTaskDelete(NULL);
        return;
    }

    // Start connection
    if (!ws_client_start())
    {
        ESP_LOGE(TAG, "Failed to start WebSocket client");
        vTaskDelete(NULL);
        return;
    }

    // Main loop - handle heartbeat and other periodic tasks
    while (1)
    {
        if (ws_client_is_connected())
        {
            // Send heartbeat if due
            if (game_state_heartbeat_due())
            {
                ws_client_send_heartbeat();
            }

            // Check respawn timer
            if (game_state_check_respawn())
            {
                ws_client_send_respawn_complete();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
