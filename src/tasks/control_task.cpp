#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "ble_weapon.h"
#include "hash.h"
#include "protocol_config.h"
#include "tasks.h"
#include "utils.h"
#include "wifi_manager.h"
#include "game_state.h"
#include "game_protocol.h"
#include "ws_client.h"

static const char* TAG = "ControlTask";

extern BLEWeapon bleWeapon;
extern QueueHandle_t laserMessageQueue;
extern uint16_t g_message_count;

void control_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Control task started");
    uint16_t data = 0;
    
    // Use device ID as the data prefix (first 8 bits encode shooter identity)
    const DeviceConfig* config = game_state_get_config();

    while (1)
    {
        // Check game state - don't shoot if respawning or game not active
        const GameStateData* state = game_state_get();
        
        if (state->state == GAME_STATE_RESPAWNING) {
            // Can't shoot while respawning
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        // In free mode, we can always shoot (even without WiFi)
        // In other modes, require WiFi connection
        if (state->mode != GAMEMODE_FREE && !wifi_manager_is_connected())
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        // Increment data counter (wraps at 255)
        data = (data + 1) & 0xFF;
        g_message_count++;
        
        // Create message with hash for validation
        uint16_t message = createMessage16bit(data);

        // Record shot in game state
        game_state_record_shot();

        // Send to laser queue
        if (xQueueSend(laserMessageQueue, &message, 0) != pdTRUE)
        {
            ESP_LOGW(TAG, "Failed to send to laser queue");
        }

        // Send via BLE to paired target(s)
        if (bleWeapon.isConnected())
        {
            bleWeapon.sendMessage(message);
            ESP_LOGI(TAG, "[BLE+Laser] %lu ms | %s | Data: %u | Shots: %lu", 
                     pdTICKS_TO_MS(xTaskGetTickCount()),
                     toBinaryString(message, MESSAGE_TOTAL_BITS).c_str(), 
                     data,
                     (unsigned long)state->shots_fired);
        }
        else
        {
            ESP_LOGI(TAG, "[Laser only] %lu ms | %s | Data: %u | Shots: %lu", 
                     pdTICKS_TO_MS(xTaskGetTickCount()),
                     toBinaryString(message, MESSAGE_TOTAL_BITS).c_str(), 
                     data,
                     (unsigned long)state->shots_fired);
        }

        // Notify server of shot (if connected)
        if (ws_client_is_connected()) {
            ws_client_send_shot_fired();
        }

        vTaskDelay(pdMS_TO_TICKS(TRANSMISSION_PAUSE_MS));
    }
}
