#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <lvgl.h>
#include "tasks.h"
#include "wifi_manager.h"

static const char* TAG = "DisplayTask";

extern bool display_initialized;
extern lv_disp_t* lvgl_display;
extern lv_obj_t* label_wifi;

void display_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Display task started");
    char line_buf[32];
    uint32_t update_counter = 0;

    while (1)
    {
        if (display_initialized && lvgl_display)
        {
            // Update text every 10 iterations (10 * 30ms = 300ms)
            if (update_counter % 10 == 0)
            {
                // Single line: IP address only
                if (wifi_manager_is_connected())
                {
                    snprintf(line_buf, sizeof(line_buf), "%s", wifi_manager_get_ip());
                }
                else
                {
                    snprintf(line_buf, sizeof(line_buf), "RayZ-Setup");
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
