#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "game_protocol.h"
#include "game_state.h"
#include "tasks.h"
#include "ws_server.h"

static const char* TAG = "GameTask";

void game_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Game task started");

    while (1)
    {
        const GameStateData* state = game_state_get();
        if (game_state_is_respawning())
        {
            if (game_state_check_respawn())
            {
                ESP_LOGI(TAG, "Respawn complete - ready to play!");
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
        }

        static uint32_t last_log = 0;
        uint32_t now = xTaskGetTickCount() / configTICK_RATE_HZ;
        if (now - last_log >= 30)
        {
            ESP_LOGI(TAG, "Stats | K/D: %lu/%lu | Shots: %lu | Hits: %lu | Hearts: %u",
                     (unsigned long)state->kills, (unsigned long)state->deaths, (unsigned long)state->shots_fired,
                     (unsigned long)state->hits_landed, (unsigned)state->hearts_remaining);
            last_log = now;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
