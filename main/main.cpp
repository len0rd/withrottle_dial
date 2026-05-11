/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_timer.h"

#include "user_config.h"
#include "user_encoder_bsp.h"
#include "ui.h"
#include "display_init.h"

#include "ParamMgr.h"
#include "Param.h"
#include "ConsoleCommands.h"
#include "wifi.h"
#include "withrottle_client.h"
#include "train_fn_screen.h"
#include <atomic>

static constexpr bool is_screen_scrollable(lv_obj_t* active_screen)
{
    return active_screen != nullptr &&
           ((active_screen == ui_Select_Train_Screen) || (active_screen == ui_Train_Main_Control) ||
            (active_screen == ui_Train_Fn_Control));
}

// Function to update settings screen values
void update_settings_screen_values()
{
    if (uic_Wifi_Value != NULL)
    {
        std::string ssid = get_current_ssid();
        lv_label_set_text(uic_Wifi_Value, ssid.c_str());
    }

    if (uic_IP_Value != NULL)
    {
        std::string ip = get_current_ip_address();
        lv_label_set_text(uic_IP_Value, ip.c_str());
    }
    if (uic_withrottle_url_value != NULL)
    {
        std::string withrottle_url = withr::get_server_url();
        if (!withrottle_url.empty())
        {
            lv_label_set_text(uic_withrottle_url_value, withrottle_url.c_str());
        }
    }
}

/// Update UI state of the main train control page
#define DIR_FWD_LABEL "dir >"
#define DIR_REV_LABEL "< dir"
void update_main_control_state()
{
    if (!withr::is_connected())
    {
        // no state to update if not connected to withrottle
        return;
    }

    static Direction s_last_direction = Direction::Forward;

    if (ui_Train_Main_direction_label != nullptr)
    {
        std::optional<Direction> current_dir = withr::get_direction();
        if (current_dir && *current_dir != s_last_direction)
        {
            s_last_direction = *current_dir;
            lv_label_set_text(ui_Train_Main_direction_label,
                              *current_dir == Direction::Forward ? DIR_FWD_LABEL : DIR_REV_LABEL);
        }
    }
}

static std::atomic<int> scroll_accumulator{0};

static const char* TAG = "main";

static void ui_update_task(void* arg)
{
    uint32_t failed_locks     = 0;
    uint32_t successful_locks = 0;

    while (1)
    {
        uint32_t start_time = esp_timer_get_time() / 1000;

        // Use 100ms timeout instead of infinite
        if (ui_lvgl_lock(100))
        {
            successful_locks++;
            failed_locks = 0; // Reset failure counter

            uint32_t lock_acquired_time = esp_timer_get_time() / 1000;

            // Update settings screen values if currently displayed
            lv_obj_t* act_scr = lv_scr_act();
            if (act_scr == ui_Settings_Screen)
            {
                update_settings_screen_values();
            }
            else if (act_scr == ui_Train_Main_Control)
            {
                update_main_control_state();
            }

            // Apply scroll from encoder
            int scroll_amount  = scroll_accumulator.load();
            scroll_accumulator = 0;
            if (scroll_amount != 0 && is_screen_scrollable(act_scr))
            {
                if (act_scr == ui_Select_Train_Screen)
                {
                    lv_obj_scroll_by(ui_Train_Select_Container, 0, scroll_amount, LV_ANIM_ON);
                }
                else if (act_scr == ui_Train_Main_Control)
                {
                    // Update throttle arc based on scroll
                    int16_t current_value = lv_arc_get_value(ui_Train_Main_Throttle);
                    int16_t min_value     = lv_arc_get_min_value(ui_Train_Main_Throttle);
                    int16_t max_value     = lv_arc_get_max_value(ui_Train_Main_Throttle);

                    // Convert scroll to throttle steps (divide to reduce sensitivity)
                    int16_t throttle_delta = -1 * (scroll_amount / 25);
                    int16_t new_value      = current_value + throttle_delta;

                    // Clamp to valid range
                    if (new_value < min_value)
                        new_value = min_value;
                    if (new_value > max_value)
                        new_value = max_value;

                    // Update the arc value
                    lv_arc_set_value(ui_Train_Main_Throttle, new_value);

                    // Convert to 0-100 percent and send to WiThrottle
                    uint8_t speed_pct =
                        (uint8_t) (((new_value - min_value) * 100) / (max_value - min_value));
                    withr::set_speed(speed_pct);
                }
                else if (act_scr == ui_Train_Fn_Control)
                {
                    static constexpr float SCROLL_SCALER = 1.5f;
                    if (scroll_amount > 0 && lv_obj_get_scroll_top(uic_train_fn_container) <= 0)
                        scroll_amount = 0;
                    if (scroll_amount < 0 && lv_obj_get_scroll_bottom(uic_train_fn_container) <= 0)
                        scroll_amount = 0;
                    if (scroll_amount != 0)
                    {
                        lv_obj_scroll_by(uic_train_fn_container, 0, SCROLL_SCALER * scroll_amount,
                                         LV_ANIM_OFF);
                    }
                }
            }

            uint32_t work_time = (esp_timer_get_time() / 1000) - lock_acquired_time;
            if (work_time > 100)
            {
                ESP_LOGW(TAG, "ui_update_task work took %lu ms (lock #%lu)", work_time,
                         successful_locks);
            }

            ui_lvgl_unlock();
        }
        else
        {
            failed_locks++;
            uint32_t wait_time = (esp_timer_get_time() / 1000) - start_time;
            ESP_LOGW(TAG,
                     "ui_update_task: Failed to acquire LVGL lock (failure #%lu, waited %lu ms)",
                     failed_locks, wait_time);

            // Check stack usage during lock failures
            UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(NULL);
            if (stack_high_water < 512)
            {
                ESP_LOGW(TAG, "ui_update_task: Low stack! High water mark: %lu bytes",
                         stack_high_water);
            }

            // If many consecutive failures, try longer delay
            if (failed_locks >= 10)
            {
                ESP_LOGE(TAG,
                         "ui_update_task: %lu consecutive lock failures - forcing longer delay",
                         failed_locks);
                vTaskDelay(pdMS_TO_TICKS(500)); // Longer delay on persistent failures
                failed_locks = 0;
            }
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

static void user_encoder_loop_task(void* arg)
{
    while (1)
    {
        EventBits_t event =
            xEventGroupWaitBits(knob_even_, BIT_EVEN_ALL, pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));

        if (READ_BIT(event, 0))
        {
            scroll_accumulator += 50;
        }
        if (READ_BIT(event, 1))
        {
            scroll_accumulator -= 50;
        }
    }
}

// Deadlock monitoring task - monitors system health and forces recovery
static void deadlock_monitor_task(void* arg)
{
    uint32_t consecutive_warnings = 0;

    ESP_LOGI(TAG, "Deadlock monitor started");

    while (1)
    {
        uint32_t current_time = esp_timer_get_time() / 1000;

        // Check if LVGL lock can be acquired (test for deadlock)
        if (ui_lvgl_lock(50)) // Short timeout
        {
            consecutive_warnings = 0;
            ui_lvgl_unlock();
        }
        else
        {
            consecutive_warnings++;
            ESP_LOGW(TAG, "DEADLOCK MONITOR: LVGL lock unavailable (warning #%lu)",
                     consecutive_warnings);

            // Force system recovery if locked for too long
            if (consecutive_warnings >= 20) // 10 seconds of lock unavailability
            {
                ESP_LOGE(TAG, "DEADLOCK MONITOR: Forcing system recovery! LVGL locked for >10s");
                ESP_LOGE(TAG, "System will restart in 3 seconds due to unrecoverable deadlock");

                // Give system a moment to log before restart
                vTaskDelay(pdMS_TO_TICKS(3000));
                ESP_LOGE(TAG, "Restarting now...");
                esp_restart();
            }
        }

        // Display overall system health every 30 seconds
        if (current_time % 30000 < 500) // Every ~30 seconds
        {
            ESP_LOGI(TAG, "System OK - LVGL responsive, warnings: %lu", consecutive_warnings);
        }

        vTaskDelay(pdMS_TO_TICKS(500)); // Check every 500ms
    }
}

extern "C" void app_main(void)
{
    ConsoleCommandsInit();
    espwifi_Init();
    ESP_LOGI(TAG, "Starting WiThrottle Knob BUILD 4 \n");

    display_init();
    ui_init();

    // remove dummy list items created by ui generator
    if (ui_lvgl_lock(1000)) // 1 second timeout for initialization
    {
        // adjust all scrollable containers to start at top
        lv_obj_scroll_by(ui_Train_Select_Container, 0, -100, LV_ANIM_OFF);
        ui_lvgl_unlock();
    }
    else
    {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock during initialization");
    }

    // initialize user encoder
    user_encoder_init();
    xTaskCreate(ui_update_task, "ui_update_task", 8 * 1024, NULL, 5, NULL);
    xTaskCreate(user_encoder_loop_task, "user_enc_task", 2 * 1024, NULL, 2, NULL);
    // xTaskCreate(deadlock_monitor_task, "deadlock_monitor", 2 * 1024, NULL, 1, NULL);

    params::ParamMgr::getInstance().listAll();

    withr::client_start();

    ESP_LOGI(TAG, "App setup complete, deleting app_main task");

    // if you delete main the whole thing dies...
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(100000));
    }
}
