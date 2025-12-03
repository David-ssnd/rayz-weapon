// this line is here in order to not break the order of the includes because freertos must be before events
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/i2c.h>
#include "ble_weapon.h"
#include "config.h"
#include "hash.h"
#include "nvs_store.h"
#include "utils.h"
#include "wifi_manager.h"


static const char* TAG = "Weapon";

BLEWeapon bleWeapon;

// FreeRTOS queues
QueueHandle_t laserMessageQueue;

// Display state
esp_lcd_panel_handle_t panel_handle = NULL;
bool display_initialized = false;
uint16_t g_message_count = 0;

void sendMessage(uint16_t message)
{
    TickType_t nextWake = xTaskGetTickCount();
    const TickType_t bitPeriod = pdMS_TO_TICKS(BIT_DURATION_MS);

    for (int i = MESSAGE_TOTAL_BITS - 1; i >= 0; i--)
    {
        bool bit = (message >> i) & 0x01;
        gpio_set_level((gpio_num_t)LASER_PIN, bit ? 1 : 0);
        vTaskDelayUntil(&nextWake, bitPeriod);
    }
    gpio_set_level((gpio_num_t)LASER_PIN, 0);
}

void control_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Control task started");
    uint16_t data = 0;

    while (1)
    {
        // Defer transmission until WiFi provisioning + STA connection is completed
        if (!wifi_manager_is_connected())
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        data++;
        g_message_count++;
        uint16_t message = createMessage16bit(data);

        // Send to laser queue
        if (xQueueSend(laserMessageQueue, &message, 0) != pdTRUE)
        {
            ESP_LOGW(TAG, "Failed to send to laser queue");
        }

        // Send via BLE
        if (bleWeapon.isConnected())
        {
            bleWeapon.sendMessage(message);
            ESP_LOGI(TAG, "[BLE+Laser] %lu ms | %s | Data: %u", pdTICKS_TO_MS(xTaskGetTickCount()),
                     toBinaryString(message, MESSAGE_TOTAL_BITS).c_str(), data);
        }
        else
        {
            ESP_LOGI(TAG, "[Laser only] %lu ms | %s | Data: %u", pdTICKS_TO_MS(xTaskGetTickCount()),
                     toBinaryString(message, MESSAGE_TOTAL_BITS).c_str(), data);
        }

        vTaskDelay(pdMS_TO_TICKS(TRANSMISSION_PAUSE_MS));
    }
}

void laser_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Laser task started");
    uint16_t message;

    while (1)
    {
        if (xQueueReceive(laserMessageQueue, &message, portMAX_DELAY) == pdTRUE)
        {
            sendMessage(message);
        }
    }
}

void ble_task(void* pvParameters)
{
    ESP_LOGI(TAG, "BLE task started");

    bleWeapon.begin();

    while (1)
    {
        bleWeapon.handleConnection();
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay
    }
}

void display_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Display task started");
    char line_buf[32];

    while (1)
    {
        if (display_initialized)
        {
            // Placeholder: Text rendering requires a font library.
            // For now, keep the display enabled. We'll integrate a font lib next.
            esp_lcd_panel_disp_on_off(panel_handle, true);
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // Update every 500ms
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Weapon device starting...");

    // Configure boot button (GPIO 0) for factory reset detection
    gpio_config_t btn_conf = {};
    btn_conf.intr_type = GPIO_INTR_DISABLE;
    btn_conf.mode = GPIO_MODE_INPUT;
    btn_conf.pin_bit_mask = (1ULL << BOOT_BUTTON_PIN);
    btn_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&btn_conf);

    // Check if boot button is pressed for 2 seconds at startup
    if (gpio_get_level((gpio_num_t)BOOT_BUTTON_PIN) == 1)
    {
        ESP_LOGW(TAG, "Boot button pressed, checking for 2 second hold...");
        vTaskDelay(pdMS_TO_TICKS(2000));

        if (gpio_get_level((gpio_num_t)BOOT_BUTTON_PIN) == 1)
        {
            ESP_LOGW(TAG, "Boot button held for 2 seconds, performing factory reset");

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
            ESP_LOGI(TAG, "Boot button released before 2 seconds, continuing normal boot");
        }
    }

    // Start WiFi provisioning / connection
    wifi_manager_init("rayz-weapon", "weapon");

    // Initialize GPIO for laser
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LASER_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)LASER_PIN, 0);

    // Create queues
    laserMessageQueue = xQueueCreate(5, sizeof(uint16_t));

    if (laserMessageQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create laser queue");
        return;
    }

    // Initialize I2C bus for OLED
    i2c_config_t i2c_conf = {};
    i2c_conf.mode = I2C_MODE_MASTER;
    i2c_conf.sda_io_num = (gpio_num_t)I2C_SDA_PIN;
    i2c_conf.scl_io_num = (gpio_num_t)I2C_SCL_PIN;
    i2c_conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_conf.master.clk_speed = 400000;
    i2c_conf.clk_flags = 0;

    esp_err_t i2c_ret = i2c_param_config(I2C_NUM_0, &i2c_conf);
    if (i2c_ret == ESP_OK)
    {
        i2c_ret = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
        if (i2c_ret == ESP_OK)
        {
            ESP_LOGI(TAG, "I2C initialized successfully");

            // Initialize SSD1306 OLED panel via esp_lcd
            esp_lcd_panel_io_handle_t io_handle = NULL;
            esp_lcd_panel_io_i2c_config_t io_config = {};
            io_config.dev_addr = OLED_I2C_ADDR;
            io_config.control_phase_bytes = 1;
            io_config.dc_bit_offset = 6;
            io_config.lcd_cmd_bits = 8;
            io_config.lcd_param_bits = 8;

            esp_err_t lcd_ret = esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_NUM_0, &io_config, &io_handle);
            if (lcd_ret == ESP_OK)
            {
                esp_lcd_panel_dev_config_t panel_config = {};
                panel_config.bits_per_pixel = 1;
                panel_config.reset_gpio_num = -1;

                lcd_ret = esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle);
                if (lcd_ret == ESP_OK)
                {
                    esp_lcd_panel_reset(panel_handle);
                    esp_lcd_panel_init(panel_handle);
                    esp_lcd_panel_disp_on_off(panel_handle, true);
                    display_initialized = true;
                    ESP_LOGI(TAG, "OLED display initialized");
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to create SSD1306 panel: %s", esp_err_to_name(lcd_ret));
                }
            }
            else
            {
                ESP_LOGE(TAG, "Failed to create panel IO: %s", esp_err_to_name(lcd_ret));
            }
        }
        else
        {
            ESP_LOGE(TAG, "Failed to install I2C driver: %s", esp_err_to_name(i2c_ret));
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to configure I2C: %s", esp_err_to_name(i2c_ret));
    }

    ESP_LOGI(TAG, "Weapon device ready");

    // Create tasks (WiFi runs independently)
    xTaskCreate(control_task, "control", 4096, NULL, 5, NULL);
    xTaskCreate(laser_task, "laser", 2048, NULL, 4, NULL);
    xTaskCreate(ble_task, "ble", 8192, NULL, 3, NULL);
    xTaskCreate(display_task, "display", 4096, NULL, 2, NULL);
}
