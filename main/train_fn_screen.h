#pragma once

#include "lvgl.h"
#include "WiThrottleProtocol.h" // MAX_FUNCTIONS

/**
 * Manages the dynamic function-button list on ui_Train_Fn_Control.
 *
 * The generated screen provides:
 *   - ui_train_fn_container : scrollable flex-column container
 *   - ui_Fn_Train_Name      : label for the locomotive name
 *   - uic_train_fn0_btn     : template button (deleted on first load)
 *
 * This class owns the 32 dynamically created function-row panels and
 * buttons.  Buttons are shown/hidden based on whether the function has
 * a non-empty name in the roster.
 */
class TrainFnScreen
{
public:
    /**
     * Call from onTrainFnControlLoaded().
     * Creates buttons on first call; refreshes labels/states on subsequent calls.
     * Must be called while holding the LVGL mutex.
     */
    static void load_for_loco();

    /// @brief TrainFn Screen update method
    /// @param scroll_amount amount of dial scroll accumulated since the last update
    static void on_screen_update(int scroll_amount);

    /**
     * Call from onTrainFnUnloaded().
     * Resets internal state so buttons are rebuilt on the next load
     * (the LVGL objects are destroyed with the screen automatically).
     */
    static void on_screen_unloaded();

private:
    static lv_obj_t* s_fn_rows[MAX_FUNCTIONS]; // panel rows, one per function slot
    static lv_obj_t* s_fn_btns[MAX_FUNCTIONS]; // buttons inside each row
    static bool      s_buttons_created;

    /** LVGL click callback — user_data is (void*)(uintptr_t)func_index */
    static void on_fn_btn_clicked(lv_event_t* e);
};
