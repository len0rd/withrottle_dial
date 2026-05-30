#pragma once

#include "lvgl.h"

class TrainMainControlScreen
{
public:
    /// Call on every ui_update when on the MainControl screen.
    static void on_screen_update(int scroll_amount);
};
