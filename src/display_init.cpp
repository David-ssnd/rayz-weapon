#include "display_init.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/i2c.h>
#include <lvgl.h>
#include "config.h"

static const char* TAG = "DisplayInit";

// Global display state (defined in main.cpp)
extern esp_lcd_panel_handle_t panel_handle;
extern bool display_initialized;
extern lv_disp_t* lvgl_display;
extern lv_obj_t* label_wifi;

// LVGL tick timer callback
static void lvgl_tick_timer_cb(void* arg)
{
    lv_tick_inc(1); // Increment LVGL tick by 1ms
}

bool init_display(void)
{
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
    if (i2c_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure I2C: %s", esp_err_to_name(i2c_ret));
        return false;
    }

    i2c_ret = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    if (i2c_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to install I2C driver: %s", esp_err_to_name(i2c_ret));
        return false;
    }

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
    if (lcd_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create panel IO: %s", esp_err_to_name(lcd_ret));
        return false;
    }

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.bits_per_pixel = 1;
    panel_config.reset_gpio_num = -1;

    // Vendor specific config for 128x32 display
    typedef struct
    {
        uint8_t height; // Panel height
    } ssd1306_vendor_config_t;
    ssd1306_vendor_config_t vendor_config = {.height = OLED_HEIGHT};
    panel_config.vendor_config = &vendor_config;

    lcd_ret = esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle);
    if (lcd_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create SSD1306 panel: %s", esp_err_to_name(lcd_ret));
        return false;
    }

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    // Initialize LVGL
    lv_init();

    // Create periodic timer for LVGL tick (1ms period)
    const esp_timer_create_args_t lvgl_tick_timer_args = {.callback = &lvgl_tick_timer_cb,
                                                          .arg = NULL,
                                                          .dispatch_method = ESP_TIMER_TASK,
                                                          .name = "lvgl_tick",
                                                          .skip_unhandled_events = false};
    esp_timer_handle_t lvgl_tick_timer = NULL;
    esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, 1000); // 1ms = 1000us

    // Create display buffer for monochrome display
    // Buffer size: full screen buffer to avoid partial update issues
    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t buf1[OLED_WIDTH * OLED_HEIGHT];
    lv_disp_draw_buf_init(&disp_buf, buf1, NULL, OLED_WIDTH * OLED_HEIGHT);

    // Register display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = OLED_WIDTH;
    disp_drv.ver_res = OLED_HEIGHT;
    disp_drv.flush_cb = [](lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_map)
    {
        if (panel_handle != NULL)
        {
            int x1 = area->x1;
            int y1 = area->y1;
            int x2 = area->x2 + 1;
            int y2 = area->y2 + 1;

            // Validate coordinates before drawing
            if (x1 >= 0 && y1 >= 0 && x2 > x1 && y2 > y1 && x2 <= OLED_WIDTH && y2 <= OLED_HEIGHT)
            {
                esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2, y2, color_map);
            }
        }
        lv_disp_flush_ready(drv);
    };
    disp_drv.draw_buf = &disp_buf;
    disp_drv.rounder_cb = [](lv_disp_drv_t* drv, lv_area_t* area)
    {
        // Round to 8-pixel page boundaries for SSD1306
        area->y1 = (area->y1 / 8) * 8;
        area->y2 = (area->y2 / 8) * 8 + 7;

        // Clamp to display bounds
        if (area->x1 < 0)
            area->x1 = 0;
        if (area->y1 < 0)
            area->y1 = 0;
        if (area->x2 >= OLED_WIDTH)
            area->x2 = OLED_WIDTH - 1;
        if (area->y2 >= OLED_HEIGHT)
            area->y2 = OLED_HEIGHT - 1;
    };
    disp_drv.set_px_cb = [](lv_disp_drv_t* drv, uint8_t* buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y,
                            lv_color_t color, lv_opa_t opa)
    {
        // SSD1306 uses vertical addressing: each byte represents 8 vertical pixels
        uint16_t byte_index = x + (y / 8) * buf_w;
        uint8_t bit_index = y % 8;

        // Set or clear the bit based on color brightness
        if (lv_color_brightness(color) > 128)
        {
            buf[byte_index] |= (1 << bit_index);
        }
        else
        {
            buf[byte_index] &= ~(1 << bit_index);
        }
    };
    lvgl_display = lv_disp_drv_register(&disp_drv);

    // Create UI elements - single centered line with built-in font
    lv_obj_t* scr = lv_disp_get_scr_act(lvgl_display);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    label_wifi = lv_label_create(scr);
    lv_label_set_text(label_wifi, "RayZ");
    lv_obj_set_style_text_color(label_wifi, lv_color_white(), 0);
    // Use default font or uncrustify (built-in monospace font that's always available)
    lv_obj_align(label_wifi, LV_ALIGN_CENTER, 0, 0);

    // Force initial render
    lv_refr_now(lvgl_display);

    display_initialized = true;
    ESP_LOGI(TAG, "OLED display initialized with LVGL");

    return true;
}
