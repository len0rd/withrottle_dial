#include "withrottle_client.h"
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
}
