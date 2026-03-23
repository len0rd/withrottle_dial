// Display initialization and LVGL porting. Taken from the 08_LVGL_Test from
// WaveShare's examples for the ESP32-S3 KNOB LCD module.
// https://www.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8#Working_with_ESP-IDF

#include <display_init.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_timer.h"

#include "lvgl.h"
#include "esp_lcd_sh8601.h"
#include "i2c_bsp.h"
#include "lcd_touch_bsp.h"
#include "user_config.h"
#include "lcd_bl_pwm_bsp.h"

static const char* TAG = "display_init";

#if CONFIG_LV_COLOR_DEPTH == 32
#define LCD_BIT_PER_PIXEL (24)
#elif CONFIG_LV_COLOR_DEPTH == 16
#define LCD_BIT_PER_PIXEL (16)
#endif
//d5-d7
static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xF0, (uint8_t[]) {0x28}, 1, 0},
    {0xF2, (uint8_t[]) {0x28}, 1, 0},
    {0x73, (uint8_t[]) {0xF0}, 1, 0},
    {0x7C, (uint8_t[]) {0xD1}, 1, 0},
    {0x83, (uint8_t[]) {0xE0}, 1, 0},
    {0x84, (uint8_t[]) {0x61}, 1, 0},
    {0xF2, (uint8_t[]) {0x82}, 1, 0},
    {0xF0, (uint8_t[]) {0x00}, 1, 0},
    {0xF0, (uint8_t[]) {0x01}, 1, 0},
    {0xF1, (uint8_t[]) {0x01}, 1, 0},
    {0xB0, (uint8_t[]) {0x56}, 1, 0},
    {0xB1, (uint8_t[]) {0x4D}, 1, 0},
    {0xB2, (uint8_t[]) {0x24}, 1, 0},
    {0xB4, (uint8_t[]) {0x87}, 1, 0},
    {0xB5, (uint8_t[]) {0x44}, 1, 0},
    {0xB6, (uint8_t[]) {0x8B}, 1, 0},
    {0xB7, (uint8_t[]) {0x40}, 1, 0},
    {0xB8, (uint8_t[]) {0x86}, 1, 0},
    {0xBA, (uint8_t[]) {0x00}, 1, 0},
    {0xBB, (uint8_t[]) {0x08}, 1, 0},
    {0xBC, (uint8_t[]) {0x08}, 1, 0},
    {0xBD, (uint8_t[]) {0x00}, 1, 0},
    {0xC0, (uint8_t[]) {0x80}, 1, 0},
    {0xC1, (uint8_t[]) {0x10}, 1, 0},
    {0xC2, (uint8_t[]) {0x37}, 1, 0},
    {0xC3, (uint8_t[]) {0x80}, 1, 0},
    {0xC4, (uint8_t[]) {0x10}, 1, 0},
    {0xC5, (uint8_t[]) {0x37}, 1, 0},
    {0xC6, (uint8_t[]) {0xA9}, 1, 0},
    {0xC7, (uint8_t[]) {0x41}, 1, 0},
    {0xC8, (uint8_t[]) {0x01}, 1, 0},
    {0xC9, (uint8_t[]) {0xA9}, 1, 0},
    {0xCA, (uint8_t[]) {0x41}, 1, 0},
    {0xCB, (uint8_t[]) {0x01}, 1, 0},
    {0xD0, (uint8_t[]) {0x91}, 1, 0},
    {0xD1, (uint8_t[]) {0x68}, 1, 0},
    {0xD2, (uint8_t[]) {0x68}, 1, 0},
    {0xF5, (uint8_t[]) {0x00, 0xA5}, 2, 0},
    {0xDD, (uint8_t[]) {0x4F}, 1, 0},
    {0xDE, (uint8_t[]) {0x4F}, 1, 0},
    {0xF1, (uint8_t[]) {0x10}, 1, 0},
    {0xF0, (uint8_t[]) {0x00}, 1, 0},
    {0xF0, (uint8_t[]) {0x02}, 1, 0},
    {0xE0,
     (uint8_t[]) {0xF0, 0x0A, 0x10, 0x09, 0x09, 0x36, 0x35, 0x33, 0x4A, 0x29, 0x15, 0x15, 0x2E,
                  0x34},
     14, 0},
    {0xE1,
     (uint8_t[]) {0xF0, 0x0A, 0x0F, 0x08, 0x08, 0x05, 0x34, 0x33, 0x4A, 0x39, 0x15, 0x15, 0x2D,
                  0x33},
     14, 0},
    {0xF0, (uint8_t[]) {0x10}, 1, 0},
    {0xF3, (uint8_t[]) {0x10}, 1, 0},
    {0xE0, (uint8_t[]) {0x07}, 1, 0},
    {0xE1, (uint8_t[]) {0x00}, 1, 0},
    {0xE2, (uint8_t[]) {0x00}, 1, 0},
    {0xE3, (uint8_t[]) {0x00}, 1, 0},
    {0xE4, (uint8_t[]) {0xE0}, 1, 0},
    {0xE5, (uint8_t[]) {0x06}, 1, 0},
    {0xE6, (uint8_t[]) {0x21}, 1, 0},
    {0xE7, (uint8_t[]) {0x01}, 1, 0},
    {0xE8, (uint8_t[]) {0x05}, 1, 0},
    {0xE9, (uint8_t[]) {0x02}, 1, 0},
    {0xEA, (uint8_t[]) {0xDA}, 1, 0},
    {0xEB, (uint8_t[]) {0x00}, 1, 0},
    {0xEC, (uint8_t[]) {0x00}, 1, 0},
    {0xED, (uint8_t[]) {0x0F}, 1, 0},
    {0xEE, (uint8_t[]) {0x00}, 1, 0},
    {0xEF, (uint8_t[]) {0x00}, 1, 0},
    {0xF8, (uint8_t[]) {0x00}, 1, 0},
    {0xF9, (uint8_t[]) {0x00}, 1, 0},
    {0xFA, (uint8_t[]) {0x00}, 1, 0},
    {0xFB, (uint8_t[]) {0x00}, 1, 0},
    {0xFC, (uint8_t[]) {0x00}, 1, 0},
    {0xFD, (uint8_t[]) {0x00}, 1, 0},
    {0xFE, (uint8_t[]) {0x00}, 1, 0},
    {0xFF, (uint8_t[]) {0x00}, 1, 0},
    {0x60, (uint8_t[]) {0x40}, 1, 0},
    {0x61, (uint8_t[]) {0x04}, 1, 0},
    {0x62, (uint8_t[]) {0x00}, 1, 0},
    {0x63, (uint8_t[]) {0x42}, 1, 0},
    {0x64, (uint8_t[]) {0xD9}, 1, 0},
    {0x65, (uint8_t[]) {0x00}, 1, 0},
    {0x66, (uint8_t[]) {0x00}, 1, 0},
    {0x67, (uint8_t[]) {0x00}, 1, 0},
    {0x68, (uint8_t[]) {0x00}, 1, 0},
    {0x69, (uint8_t[]) {0x00}, 1, 0},
    {0x6A, (uint8_t[]) {0x00}, 1, 0},
    {0x6B, (uint8_t[]) {0x00}, 1, 0},
    {0x70, (uint8_t[]) {0x40}, 1, 0},
    {0x71, (uint8_t[]) {0x03}, 1, 0},
    {0x72, (uint8_t[]) {0x00}, 1, 0},
    {0x73, (uint8_t[]) {0x42}, 1, 0},
    {0x74, (uint8_t[]) {0xD8}, 1, 0},
    {0x75, (uint8_t[]) {0x00}, 1, 0},
    {0x76, (uint8_t[]) {0x00}, 1, 0},
    {0x77, (uint8_t[]) {0x00}, 1, 0},
    {0x78, (uint8_t[]) {0x00}, 1, 0},
    {0x79, (uint8_t[]) {0x00}, 1, 0},
    {0x7A, (uint8_t[]) {0x00}, 1, 0},
    {0x7B, (uint8_t[]) {0x00}, 1, 0},
    {0x80, (uint8_t[]) {0x48}, 1, 0},
    {0x81, (uint8_t[]) {0x00}, 1, 0},
    {0x82, (uint8_t[]) {0x06}, 1, 0},
    {0x83, (uint8_t[]) {0x02}, 1, 0},
    {0x84, (uint8_t[]) {0xD6}, 1, 0},
    {0x85, (uint8_t[]) {0x04}, 1, 0},
    {0x86, (uint8_t[]) {0x00}, 1, 0},
    {0x87, (uint8_t[]) {0x00}, 1, 0},
    {0x88, (uint8_t[]) {0x48}, 1, 0},
    {0x89, (uint8_t[]) {0x00}, 1, 0},
    {0x8A, (uint8_t[]) {0x08}, 1, 0},
    {0x8B, (uint8_t[]) {0x02}, 1, 0},
    {0x8C, (uint8_t[]) {0xD8}, 1, 0},
    {0x8D, (uint8_t[]) {0x04}, 1, 0},
    {0x8E, (uint8_t[]) {0x00}, 1, 0},
    {0x8F, (uint8_t[]) {0x00}, 1, 0},
    {0x90, (uint8_t[]) {0x48}, 1, 0},
    {0x91, (uint8_t[]) {0x00}, 1, 0},
    {0x92, (uint8_t[]) {0x0A}, 1, 0},
    {0x93, (uint8_t[]) {0x02}, 1, 0},
    {0x94, (uint8_t[]) {0xDA}, 1, 0},
    {0x95, (uint8_t[]) {0x04}, 1, 0},
    {0x96, (uint8_t[]) {0x00}, 1, 0},
    {0x97, (uint8_t[]) {0x00}, 1, 0},
    {0x98, (uint8_t[]) {0x48}, 1, 0},
    {0x99, (uint8_t[]) {0x00}, 1, 0},
    {0x9A, (uint8_t[]) {0x0C}, 1, 0},
    {0x9B, (uint8_t[]) {0x02}, 1, 0},
    {0x9C, (uint8_t[]) {0xDC}, 1, 0},
    {0x9D, (uint8_t[]) {0x04}, 1, 0},
    {0x9E, (uint8_t[]) {0x00}, 1, 0},
    {0x9F, (uint8_t[]) {0x00}, 1, 0},
    {0xA0, (uint8_t[]) {0x48}, 1, 0},
    {0xA1, (uint8_t[]) {0x00}, 1, 0},
    {0xA2, (uint8_t[]) {0x05}, 1, 0},
    {0xA3, (uint8_t[]) {0x02}, 1, 0},
    {0xA4, (uint8_t[]) {0xD5}, 1, 0},
    {0xA5, (uint8_t[]) {0x04}, 1, 0},
    {0xA6, (uint8_t[]) {0x00}, 1, 0},
    {0xA7, (uint8_t[]) {0x00}, 1, 0},
    {0xA8, (uint8_t[]) {0x48}, 1, 0},
    {0xA9, (uint8_t[]) {0x00}, 1, 0},
    {0xAA, (uint8_t[]) {0x07}, 1, 0},
    {0xAB, (uint8_t[]) {0x02}, 1, 0},
    {0xAC, (uint8_t[]) {0xD7}, 1, 0},
    {0xAD, (uint8_t[]) {0x04}, 1, 0},
    {0xAE, (uint8_t[]) {0x00}, 1, 0},
    {0xAF, (uint8_t[]) {0x00}, 1, 0},
    {0xB0, (uint8_t[]) {0x48}, 1, 0},
    {0xB1, (uint8_t[]) {0x00}, 1, 0},
    {0xB2, (uint8_t[]) {0x09}, 1, 0},
    {0xB3, (uint8_t[]) {0x02}, 1, 0},
    {0xB4, (uint8_t[]) {0xD9}, 1, 0},
    {0xB5, (uint8_t[]) {0x04}, 1, 0},
    {0xB6, (uint8_t[]) {0x00}, 1, 0},
    {0xB7, (uint8_t[]) {0x00}, 1, 0},
    {0xB8, (uint8_t[]) {0x48}, 1, 0},
    {0xB9, (uint8_t[]) {0x00}, 1, 0},
    {0xBA, (uint8_t[]) {0x0B}, 1, 0},
    {0xBB, (uint8_t[]) {0x02}, 1, 0},
    {0xBC, (uint8_t[]) {0xDB}, 1, 0},
    {0xBD, (uint8_t[]) {0x04}, 1, 0},
    {0xBE, (uint8_t[]) {0x00}, 1, 0},
    {0xBF, (uint8_t[]) {0x00}, 1, 0},
    {0xC0, (uint8_t[]) {0x10}, 1, 0},
    {0xC1, (uint8_t[]) {0x47}, 1, 0},
    {0xC2, (uint8_t[]) {0x56}, 1, 0},
    {0xC3, (uint8_t[]) {0x65}, 1, 0},
    {0xC4, (uint8_t[]) {0x74}, 1, 0},
    {0xC5, (uint8_t[]) {0x88}, 1, 0},
    {0xC6, (uint8_t[]) {0x99}, 1, 0},
    {0xC7, (uint8_t[]) {0x01}, 1, 0},
    {0xC8, (uint8_t[]) {0xBB}, 1, 0},
    {0xC9, (uint8_t[]) {0xAA}, 1, 0},
    {0xD0, (uint8_t[]) {0x10}, 1, 0},
    {0xD1, (uint8_t[]) {0x47}, 1, 0},
    {0xD2, (uint8_t[]) {0x56}, 1, 0},
    {0xD3, (uint8_t[]) {0x65}, 1, 0},
    {0xD4, (uint8_t[]) {0x74}, 1, 0},
    {0xD5, (uint8_t[]) {0x88}, 1, 0},
    {0xD6, (uint8_t[]) {0x99}, 1, 0},
    {0xD7, (uint8_t[]) {0x01}, 1, 0},
    {0xD8, (uint8_t[]) {0xBB}, 1, 0},
    {0xD9, (uint8_t[]) {0xAA}, 1, 0},
    {0xF3, (uint8_t[]) {0x01}, 1, 0},
    {0xF0, (uint8_t[]) {0x00}, 1, 0},
    {0x21, (uint8_t[]) {0x00}, 1, 0},
    {0x11, (uint8_t[]) {0x00}, 1, 120},
    {0x29, (uint8_t[]) {0x00}, 1, 0},
#ifdef EXAMPLE_Rotate_90
    {0x36, (uint8_t[]) {0x60}, 1, 0},
#else
    {0x36, (uint8_t[]) {0x00}, 1, 0},
#endif
};

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map)
{
    static uint32_t flush_count = 0;
    flush_count++;

    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) lv_display_get_user_data(disp);
    const int              offsetx1     = area->x1;
    const int              offsetx2     = area->x2;
    const int              offsety1     = area->y1;
    const int              offsety2     = area->y2;

    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);

    // always call ready even if draw failed
    lv_display_flush_ready(disp);
}

static void lvgl_rounder_cb(lv_event_t* e)
{
    lv_area_t* area = (lv_area_t*) lv_event_get_param(e);

    // Round the start of coordinate down to the nearest 2M number
    area->x1 = (area->x1 >> 1) << 1;
    area->y1 = (area->y1 >> 1) << 1;
    // Round the end of coordinate up to the nearest 2N+1 number
    area->x2 = ((area->x2 >> 1) << 1) + 1;
    area->y2 = ((area->y2 >> 1) << 1) + 1;
}

// Touch event structure for queue communication
typedef struct
{
    uint16_t         x;
    uint16_t         y;
    lv_indev_state_t state;
    bool             valid;
    uint32_t         lvgl_ts;
} touch_event_t;

// Touch event queue - non-blocking communication from touch_reader_task to callback
static QueueHandle_t touch_queue = NULL;

static void touch_reader_task(void* arg)
{
    uint32_t touch_count  = 0;
    uint32_t failed_reads = 0;
    uint32_t last_valid_x = 0, last_valid_y = 0;
    uint32_t cycles_count = 0;

    ESP_LOGI(TAG, "Touch reader task started");

    while (1)
    {
        touch_event_t touch_event = {};

        uint32_t start_time = esp_timer_get_time() / 1000;

        // Perform I2C touch read WITHOUT holding LVGL mutex
        touch_event.lvgl_ts = lv_tick_get();
        uint8_t win         = tpGetCoordinates(&touch_event.x, &touch_event.y);

        uint32_t end_time = esp_timer_get_time() / 1000;
        uint32_t duration = end_time - start_time;
        touch_count++;
        cycles_count++;

        // Log slow I2C operations
        if (duration > 10)
        {
            ESP_LOGW(TAG, "Touch I2C read took %lu ms (read #%lu)", duration, touch_count);
            failed_reads++;
        }

        // Prepare touch event

        if (win && duration < 50)
        { // Valid reading
            failed_reads = 0;
            if (touch_event.x > LCD_H_RES)
                touch_event.x = LCD_H_RES;
            if (touch_event.y > LCD_V_RES)
                touch_event.y = LCD_V_RES;

            touch_event.state = LV_INDEV_STATE_PRESSED;
            touch_event.valid = true;
            last_valid_x      = touch_event.x;
            last_valid_y      = touch_event.y;
        }
        else
        {
            // No touch or failed read
            touch_event.x     = last_valid_x;
            touch_event.y     = last_valid_y;
            touch_event.state = LV_INDEV_STATE_RELEASED;
            touch_event.valid = true;
        }

        // Handle persistent failures
        if (failed_reads > 10)
        {
            ESP_LOGW(TAG, "Touch interface unstable (%lu failures) - using fallback", failed_reads);
            touch_event.state = LV_INDEV_STATE_RELEASED;
            touch_event.valid = false;
            failed_reads      = 0;
        }

        // Send touch event to queue (non-blocking)
        // If queue is full, overwrite oldest entry
        if (xQueueSend(touch_queue, &touch_event, 0) != pdTRUE)
        {
            // Queue full - remove oldest and add new
            touch_event_t dummy;
            xQueueReceive(touch_queue, &dummy, 0);
            xQueueSend(touch_queue, &touch_event, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // Read every 20ms
    }
}

static void screen_lvgl_touch_cb(lv_indev_t* drv, lv_indev_data_t* data)
{
    static touch_event_t last_event = {};
    last_event.x                    = 0;
    last_event.y                    = 0;
    last_event.state                = LV_INDEV_STATE_RELEASED;
    last_event.valid                = true;
    touch_event_t current_event;

    // Try to get latest touch event from queue
    if (xQueueReceive(touch_queue, &current_event, 0) == pdTRUE)
    {
        last_event = current_event;
        // Check if queue has more events - tell LVGL to call us again immediately
        data->continue_reading = (uxQueueMessagesWaiting(touch_queue) > 0);
    }
    else
    {
        // default to released if unable to retrieve new data
        last_event.state = LV_INDEV_STATE_RELEASED;
    }

    data->point.x   = last_event.x;
    data->point.y   = last_event.y;
    data->state     = last_event.state;
    data->timestamp = last_event.lvgl_ts;
}

static void example_increase_lvgl_tick(void* arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static SemaphoreHandle_t ui_sem                = NULL;
static uint32_t          ui_sem_lock_count     = 0;
static TaskHandle_t      ui_sem_owner          = NULL;
static uint32_t          ui_sem_last_lock_time = 0;

bool ui_lvgl_lock(int timeout_ms)
{
    assert(ui_sem && "display_init must be called first");

    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    uint32_t         start_time    = esp_timer_get_time() / 1000;

    bool acquired = xSemaphoreTake(ui_sem, timeout_ticks) == pdTRUE;

    if (acquired)
    {
        ui_sem_lock_count++;
        ui_sem_owner          = xTaskGetCurrentTaskHandle();
        ui_sem_last_lock_time = start_time;

        // Check for potential deadlock (held > 5 seconds)
        if (start_time - ui_sem_last_lock_time > 5000)
        {
            ESP_LOGW(TAG, "DEADLOCK WARNING: LVGL mutex held for %lu ms by task %s",
                     start_time - ui_sem_last_lock_time,
                     ui_sem_owner ? pcTaskGetName(ui_sem_owner) : "unknown");
        }
    }
    else
    {
        uint32_t wait_time = (esp_timer_get_time() / 1000) - start_time;
        ESP_LOGW(TAG, "LVGL lock failed after %lu ms timeout (requested %dms). Current owner: %s",
                 wait_time, timeout_ms, ui_sem_owner ? pcTaskGetName(ui_sem_owner) : "none");
    }

    return acquired;
}

void ui_lvgl_unlock(void)
{
    assert(ui_sem && "bsp_display_start must be called first");

    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t hold_time    = current_time - ui_sem_last_lock_time;

    if (hold_time > 1000)
    { // Log if held > 1 second
        ESP_LOGW(TAG, "LVGL mutex held for %lu ms (lock #%lu)", hold_time, ui_sem_lock_count);
    }

    ui_sem_owner = NULL;
    xSemaphoreGive(ui_sem);
}

static void example_lvgl_port_task(void* arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;

    while (1)
    {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        // Use timeout instead of infinite wait
        if (ui_lvgl_lock(100)) // 100ms timeout
        {
            task_delay_ms = lv_task_handler();
            // Release the mutex
            ui_lvgl_unlock();
        }
        else
        {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }

        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS)
        {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        }
        else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS)
        {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

void display_init(void)
{
    static lv_display_t* s_display = NULL;

    ESP_LOGI(TAG, "Starting display initialization");

    // Enable debug logging for this component
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);

    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .data0_io_num    = EXAMPLE_PIN_NUM_LCD_DATA0,
        .data1_io_num    = EXAMPLE_PIN_NUM_LCD_DATA1,
        .sclk_io_num     = EXAMPLE_PIN_NUM_LCD_PCLK,
        .data2_io_num    = EXAMPLE_PIN_NUM_LCD_DATA2,
        .data3_io_num    = EXAMPLE_PIN_NUM_LCD_DATA3,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t           io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config =
        SH8601_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, NULL, NULL);
    sh8601_vendor_config_t vendor_config = {
        .init_cmds      = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags =
            {
                .use_qspi_interface = 1,
            },
    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t) LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_handle_t     panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num             = EXAMPLE_PIN_NUM_LCD_RST;
    panel_config.rgb_ele_order              = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel             = LCD_BIT_PER_PIXEL;
    panel_config.vendor_config              = &vendor_config;
    ESP_LOGI(TAG, "Install SH8601 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    i2c_master_Init(); //I2C_Init

    ESP_LOGI(TAG, "Touch interface enabled - initializing...");
    lcd_touch_init();
    ESP_LOGI(TAG, "Touch interface initialized");

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    s_display = lv_display_create(LCD_H_RES, LCD_V_RES);
    if (!s_display)
    {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        return;
    }

    // Statically allocate draw buffers in DMA-capable internal RAM
    // Note: PSRAM cannot be used with SPI LCD DMA transfers
    static constexpr size_t BUFF_SIZE = LCD_H_RES * LVGL_BUF_HEIGHT * sizeof(lv_color_t);
    static DMA_ATTR uint8_t buf1[BUFF_SIZE];
    static DMA_ATTR uint8_t buf2[BUFF_SIZE];

    // size_t buf_size = sizeof(buf1);
    // ESP_LOGI(TAG, "Using static DMA buffers: %zu bytes each (total %zu bytes)", buf_size,
    //          buf_size * 2);

    // Clear the buffers
    memset(buf1, 0, BUFF_SIZE);
    memset(buf2, 0, BUFF_SIZE);

    lv_display_set_buffers(s_display, buf1, buf2, BUFF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_display, lvgl_flush_cb);
    lv_display_set_user_data(s_display, panel_handle);
    lv_display_add_event_cb(s_display, lvgl_rounder_cb, LV_EVENT_INVALIDATE_AREA, NULL);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    //Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback              = &example_increase_lvgl_tick,
        .arg                   = NULL,
        .dispatch_method       = ESP_TIMER_TASK,
        .name                  = "lvgl_tick",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    // setup reading touch sensor
    ESP_LOGI(TAG, "Registering touch input device...");
    static lv_indev_t* s_touch_indev = lv_indev_create();
    if (!s_touch_indev)
    {
        ESP_LOGE(TAG, "Failed to create LVGL touch input device");
        return;
    }
    lv_indev_set_type(s_touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_touch_indev, screen_lvgl_touch_cb);

    // Initialize touch event queue for non-blocking communication
    touch_queue = xQueueCreate(3, sizeof(touch_event_t)); // Queue depth of 3 touch events
    assert(touch_queue);

    // Start touch reader task with priority between UI tasks and LVGL
    xTaskCreate(touch_reader_task, "touch_reader", 6 * 1024, NULL, 4, NULL);
    ESP_LOGI(TAG, "Touch reader task started");

    ui_sem = xSemaphoreCreateMutex();
    assert(ui_sem);
    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL,
                EXAMPLE_LVGL_TASK_PRIORITY, NULL);
}
