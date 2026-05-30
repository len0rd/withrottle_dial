#include "train_main_control_screen.h"
#include "withrottle_client.h"
#include "ui.h"

/// Update UI state of the main train control page
#define DIR_FWD_LABEL "dir >"
#define DIR_REV_LABEL "< dir"
static void update_main_control_state()
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

static void apply_scroll(int scroll_amount)
{
    if (scroll_amount == 0)
    {
        return;
    }

    // Update throttle arc based on scroll
    int16_t current_value = lv_arc_get_value(ui_Train_Main_Throttle);
    int16_t min_value     = lv_arc_get_min_value(ui_Train_Main_Throttle);
    int16_t max_value     = lv_arc_get_max_value(ui_Train_Main_Throttle);

    // Convert scroll to throttle steps (divide to reduce sensitivity)
    int16_t throttle_delta = -1 * (scroll_amount / 25);
    int16_t new_value      = current_value + throttle_delta;

    // Clamp to valid range
    new_value = std::max(min_value, std::min(max_value, new_value));

    // Update the arc value
    lv_arc_set_value(ui_Train_Main_Throttle, new_value);

    // Convert to 0-100 percent and send to WiThrottle
    uint8_t speed_pct = (uint8_t) (((new_value - min_value) * 100) / (max_value - min_value));
    withr::set_speed(speed_pct);
}

void TrainMainControlScreen::on_screen_update(int scroll_amount)
{
    update_main_control_state();
    apply_scroll(scroll_amount);
}
