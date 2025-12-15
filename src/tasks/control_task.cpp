#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include "config.h"
#include "espnow_comm.h"
#include "game_protocol.h"
#include "game_state.h"
#include "hash.h"
#include "protocol_config.h"
#include "tasks.h"
#include "utils.h"
#include "wifi_manager.h"
#include "ws_server.h"

static const char* TAG = "ControlTask";
static bool s_trigger_initialized = false;

extern QueueHandle_t laserMessageQueue;
static uint16_t g_message_count = 0;

static void init_trigger_button(void)
{
    if (s_trigger_initialized)
        return;

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << TRIGGER_BUTTON_PIN);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE; // Button pulls to GND when pressed
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    s_trigger_initialized = true;
    ESP_LOGI(TAG, "Trigger button initialized on GPIO %d", TRIGGER_BUTTON_PIN);
}

static bool is_trigger_pressed(void)
{
    // Button is active LOW (pressed = 0)
    return gpio_get_level((gpio_num_t)TRIGGER_BUTTON_PIN) == 0;
}

void control_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Control task started");
    uint8_t shot_counter = 0;
    bool was_pressed = false;

    init_trigger_button();

    while (1)
    {
        const DeviceConfig* config = game_state_get_config();
        const GameConfig* gcfg = game_state_get_game_config();

        if (game_state_is_respawning())
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!gcfg->unlimited_ammo && gcfg->max_ammo == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        bool is_pressed = is_trigger_pressed();
        if (!is_pressed)
        {
            was_pressed = false;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (was_pressed)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        was_pressed = true;

        shot_counter++;
        g_message_count++;

        uint32_t laser_msg = createLaserMessage(config->player_id, config->device_id);

        game_state_record_shot();

        PlayerMessage shot_msg = {};
        shot_msg.type = ESPNOW_MSG_SHOT;
        shot_msg.version = 1;
        shot_msg.player_id = config->player_id;
        shot_msg.device_id = config->device_id;
        shot_msg.team_id = config->team_id;
        shot_msg.color_rgb = config->color_rgb;
        shot_msg.data = laser_msg;
        shot_msg.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
        if (!espnow_comm_broadcast(&shot_msg))
        {
            ESP_LOGW(TAG, "ESP-NOW shot broadcast failed");
        }

        if (xQueueSend(laserMessageQueue, &laser_msg, 0) != pdTRUE)
        {
            ESP_LOGW(TAG, "Failed to send to laser queue");
        }

        ESP_LOGI(TAG, "[Laser] %lu ms | %s | Shots: %lu", pdTICKS_TO_MS(xTaskGetTickCount()),
                 toBinaryString(laser_msg, MESSAGE_TOTAL_BITS).c_str(),
                 (unsigned long)game_state_get()->shots_fired);

        if (ws_server_is_connected())
        {
            ws_server_broadcast_shot();
        }

        vTaskDelay(pdMS_TO_TICKS(TRANSMISSION_PAUSE_MS));
    }
}
