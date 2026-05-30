#include "train_fn_screen.h"

#include "withrottle_client.h"
#include "ui.h"

#include <string>

// ---- Static member definitions --------------------------------------------

lv_obj_t* TrainFnScreen::s_fn_rows[MAX_FUNCTIONS] = {};
lv_obj_t* TrainFnScreen::s_fn_btns[MAX_FUNCTIONS] = {};
bool      TrainFnScreen::s_buttons_created        = false;

// ---- Private helpers ------------------------------------------------------

void TrainFnScreen::on_fn_btn_clicked(lv_event_t* e)
{
    uint8_t func = (uint8_t) (uintptr_t) lv_event_get_user_data(e);

    // LV_EVENT_CLICKED fires after LVGL has already toggled LV_STATE_CHECKED,
    // so reading it now gives the new desired state.
    lv_obj_t* btn    = lv_event_get_target(e);
    bool      now_on = lv_obj_has_state(btn, LV_STATE_CHECKED);
    withr::set_function(func, now_on);
}

// ---- Public API -----------------------------------------------------------

void TrainFnScreen::load_for_loco()
{
    // Guard: container must be valid (set by generated screen init)
    if (uic_train_fn_container == nullptr)
        return;

    // --- Update loco name label ---
    if (ui_Fn_Train_Name != nullptr)
    {
        std::optional<std::string> name = withr::get_loco_name();
        lv_label_set_text(ui_Fn_Train_Name, name ? name->c_str() : "\xe2\x80\x94");
    }

    // --- Create buttons on first load ---
    if (!s_buttons_created)
    {
        for (uint8_t i = 0; i < MAX_FUNCTIONS; i++)
        {
            // Row panel
            lv_obj_t* row = lv_obj_create(uic_train_fn_container);
            lv_obj_set_size(row, 360, 50);
            lv_obj_set_align(row, LV_ALIGN_CENTER);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_opa(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

            // Function button
            lv_obj_t* btn = lv_btn_create(row);
            lv_obj_set_size(btn, 280, 44);
            lv_obj_set_align(btn, LV_ALIGN_CENTER);
            lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

            // Default (off) appearance — Secondary theme colour
            ui_object_set_themeable_style_property(btn, LV_PART_MAIN | LV_STATE_DEFAULT,
                                                   LV_STYLE_BG_COLOR, _ui_theme_color_Secondary);
            ui_object_set_themeable_style_property(btn, LV_PART_MAIN | LV_STATE_DEFAULT,
                                                   LV_STYLE_BG_OPA, _ui_theme_alpha_Secondary);

            // Checked (on) appearance — Standout theme colour
            ui_object_set_themeable_style_property(btn, LV_PART_MAIN | LV_STATE_CHECKED,
                                                   LV_STYLE_BG_COLOR, _ui_theme_color_Standout);
            ui_object_set_themeable_style_property(btn, LV_PART_MAIN | LV_STATE_CHECKED,
                                                   LV_STYLE_BG_OPA, _ui_theme_alpha_Standout);

            // Label
            lv_obj_t* label = lv_label_create(btn);
            lv_obj_set_align(label, LV_ALIGN_CENTER);
            lv_obj_set_width(label, LV_SIZE_CONTENT);
            lv_obj_set_height(label, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_20,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(label, "");

            // Click callback — pass function index as user_data
            lv_obj_add_event_cb(btn, on_fn_btn_clicked, LV_EVENT_CLICKED, (void*) (uintptr_t) i);

            s_fn_rows[i] = row;
            s_fn_btns[i] = btn;
        }

        s_buttons_created = true;
    }

    // --- Refresh visibility, labels, and toggle state ---
    for (uint8_t i = 0; i < MAX_FUNCTIONS; i++)
    {
        lv_obj_t* row = s_fn_rows[i];
        lv_obj_t* btn = s_fn_btns[i];
        if (row == nullptr || btn == nullptr)
            continue;

        std::optional<std::string> fn_name  = withr::get_function_name(i);
        bool                       has_name = fn_name && !fn_name->empty();

        if (!has_name)
        {
            lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        lv_obj_clear_flag(row, LV_OBJ_FLAG_HIDDEN);

        // Update label (first child of button)
        lv_obj_t* label = lv_obj_get_child(btn, 0);
        if (label != nullptr)
            lv_label_set_text(label, fn_name->c_str());

        // Sync toggle state
        std::optional<bool> state = withr::get_function_state(i);
        if (state && *state)
            lv_obj_add_state(btn, LV_STATE_CHECKED);
        else
            lv_obj_clear_state(btn, LV_STATE_CHECKED);
    }
}

void TrainFnScreen::on_screen_update(int scroll_amount)
{
    if (scroll_amount == 0)
    {
        return;
    }
    static constexpr float SCROLL_SCALER = 1.5f;
    if (scroll_amount > 0 && lv_obj_get_scroll_top(uic_train_fn_container) <= 0)
        scroll_amount = 0;
    if (scroll_amount < 0 && lv_obj_get_scroll_bottom(uic_train_fn_container) <= 0)
        scroll_amount = 0;
    if (scroll_amount != 0)
    {
        lv_obj_scroll_by(uic_train_fn_container, 0, SCROLL_SCALER * scroll_amount, LV_ANIM_OFF);
    }
}

void TrainFnScreen::on_screen_unloaded()
{
    // LVGL objects are destroyed with the screen; just reset our tracking state
    // so buttons are rebuilt on the next load.
    for (int i = 0; i < MAX_FUNCTIONS; i++)
    {
        s_fn_rows[i] = nullptr;
        s_fn_btns[i] = nullptr;
    }
    s_buttons_created = false;
}
