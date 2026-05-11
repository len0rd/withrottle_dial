#include "withrottle_client.h"
#include "train_fn_screen.h"
#include "ui.h"

extern "C"
{
    void onStopClicked(lv_event_t* e)
    {
        withr::emergency_stop();
    }

    void onDirClicked(lv_event_t* e)
    {
        std::optional<Direction> current = withr::get_direction();
        if (current)
        {
            withr::set_direction(current == Direction::Forward ? Direction::Reverse
                                                               : Direction::Forward);
        }
    }

    void onTrainMainControlLoaded(lv_event_t* e)
    {
        if (ui_Train_Main_Name_Label != nullptr)
        {
            std::optional<std::string> train_name = withr::get_loco_name();
            if (train_name && !train_name->empty())
            {
                lv_label_set_text(ui_Train_Main_Name_Label, train_name->c_str());
            }
        }
    }

    void onTrainFnControlLoaded(lv_event_t* e)
    {
        TrainFnScreen::load_for_loco();
    }

    void onTrainFnUnloaded(lv_event_t* e)
    {
        TrainFnScreen::on_screen_unloaded();
    }
}
