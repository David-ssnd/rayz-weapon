#include "gpio_init.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <nvs_flash.h>
#include "config.h"
#include "wifi_manager.h"

static const char* TAG = "GPIOInit";

void init_reset_button_and_check_factory_reset(void)
{
    // Configure reset button (GPIO 0) for factory reset detection
    gpio_config_t btn_conf = {};
    btn_conf.intr_type = GPIO_INTR_DISABLE;
    btn_conf.mode = GPIO_MODE_INPUT;
    btn_conf.pin_bit_mask = (1ULL << RESET_BUTTON_PIN);
    btn_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&btn_conf);

    // Check if reset button is pressed for 2 seconds at startup
    if (gpio_get_level((gpio_num_t)RESET_BUTTON_PIN) == 1)
    {
        ESP_LOGW(TAG, "Reset button pressed, checking for 2 second hold...");
        vTaskDelay(pdMS_TO_TICKS(2000));

        if (gpio_get_level((gpio_num_t)RESET_BUTTON_PIN) == 1)
        {
            ESP_LOGW(TAG, "Reset button held for 2 seconds, performing factory reset");

            // Initialize NVS first (required for factory reset)
            esp_err_t ret = nvs_flash_init();
            if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
            {
                nvs_flash_erase();
                nvs_flash_init();
            }

            wifi_manager_factory_reset();
            // Note: esp_restart() is called, execution stops here
        }
        else
        {
            ESP_LOGI(TAG, "Reset button released before 2 seconds, continuing normal boot");
        }
    }
}

void init_laser_gpio(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LASER_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)LASER_PIN, 0);

    ESP_LOGI(TAG, "Laser GPIO initialized on pin %d", LASER_PIN);
}
