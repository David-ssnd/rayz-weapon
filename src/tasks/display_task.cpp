#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <lvgl.h>
#include "tasks.h"
#include "wifi_manager.h"
#include "game_state.h"
#include "game_protocol.h"

static const char* TAG = "DisplayTask";

extern bool display_initialized;
extern lv_disp_t* lvgl_display;
extern lv_obj_t* label_wifi;

void display_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Display task started");
    char line_buf[64];
    uint32_t update_counter = 0;

    while (1)
    {
        if (display_initialized && lvgl_display)
        {
            // Update text every 10 iterations (10 * 30ms = 300ms)
            if (update_counter % 10 == 0)
            {
                const GameStateData* state = game_state_get();
                const DeviceConfig* config = game_state_get_config();
                
                if (wifi_manager_is_connected())
                {
                    // Show game info when connected
                    if (state->state == GAME_STATE_RESPAWNING) {
                        snprintf(line_buf, sizeof(line_buf), "RESPAWNING...");
                    } else {
                        // Format: K:0 D:0 | free
                        snprintf(line_buf, sizeof(line_buf), "K:%lu D:%lu | %s",
                                 (unsigned long)state->kills,
                                 (unsigned long)state->deaths,
                                 GAMEMODE_NAMES[state->mode]);
                    }
                }
                else
                {
                    snprintf(line_buf, sizeof(line_buf), "RayZ-Setup: 192.168.4.1");
                }
                lv_label_set_text(label_wifi, line_buf);
            }

            // Handle LVGL timer tasks (must be called frequently for display refresh)
            lv_timer_handler();
            update_counter++;
        }

        vTaskDelay(pdMS_TO_TICKS(30)); // LVGL refresh rate
    }
}
