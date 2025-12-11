#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "game_protocol.h"
#include "game_state.h"
#include "tasks.h"
#include "ws_client.h"


static const char* TAG = "GameTask";

extern QueueHandle_t laserMessageQueue;

void game_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Game task started");

    const GameStateData* state = game_state_get();

    while (1)
    {
        // Handle respawn state
        if (state->state == GAME_STATE_RESPAWNING)
        {
            if (game_state_check_respawn())
            {
                ESP_LOGI(TAG, "Respawn complete - ready to play!");
            }
            else
            {
                // Still respawning - skip game logic
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
        }

        // Handle different game modes
        switch (state->mode)
        {
            case GAMEMODE_FREE:
                // Free mode: always allow shooting
                // No special handling needed - control_task handles shooting
                break;

            case GAMEMODE_DEATHMATCH:
                // Deathmatch: everyone is enemy
                // Check win condition (e.g., first to 10 kills)
                if (state->kills >= 10)
                {
                    ESP_LOGI(TAG, "Deathmatch goal reached!");
                    // Server will handle game end
                }
                break;

            case GAMEMODE_TEAM:
                // Team mode: friendly fire ignored
                // Server handles team logic
                break;

            case GAMEMODE_CAPTURE_FLAG:
                // CTF: special objectives
                // TODO: implement flag logic
                break;

            case GAMEMODE_TIMED:
                // Timed: check remaining time
                // Server tracks time
                break;

            default:
                break;
        }

        // Log stats periodically (every 30 seconds)
        static uint32_t last_log = 0;
        uint32_t now = xTaskGetTickCount() / configTICK_RATE_HZ;
        if (now - last_log >= 30)
        {
            ESP_LOGI(TAG, "Stats - Mode: %s | State: %s | K/D: %lu/%lu | Shots: %lu | Hits: %lu",
                     GAMEMODE_NAMES[state->mode], GAME_STATE_NAMES[state->state], (unsigned long)state->kills,
                     (unsigned long)state->deaths, (unsigned long)state->shots_fired,
                     (unsigned long)state->hits_landed);
            last_log = now;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
