#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <stdio.h>
#include "display_manager.h"
#include "game_state.h"
#include "tasks.h"
#include "wifi_manager.h"
#include "ws_server.h"


static const char* TAG = "WsTask";

static void ws_on_connect(int client_fd, bool connected)
{
    int count = ws_server_client_count();
    ESP_LOGI(TAG, "WebSocket %s (fd=%d, total=%d)", connected ? "connected" : "disconnected", client_fd, count);

    dm_event_t evt = {};
    evt.type = DM_EVT_MSG;
    snprintf(evt.msg.text, sizeof(evt.msg.text), connected ? "WS ON" : "WS OFF");
    display_manager_post(&evt);
}

void ws_task(void* pvParameters)
{
    ESP_LOGI(TAG, "WebSocket task started");

    WsServerConfig ws_cfg = {};
    ws_cfg.on_connect = ws_on_connect;
    ws_server_init(&ws_cfg);

    while (!wifi_manager_is_connected())
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI(TAG, "WiFi connected, WebSocket server already running");

    while (1)
    {
        // Cleanup stale clients (handles browser refresh without close frame)
        ws_server_cleanup_stale();

        if (ws_server_is_connected() && game_state_heartbeat_due())
        {
            ws_server_send_status();
        }

        if (game_state_check_respawn())
        {
            ws_server_broadcast_respawn();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
