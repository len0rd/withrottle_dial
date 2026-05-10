#include "withrottle_client.h"
#include "ui.h"

extern "C"
{
    void onStopClicked(lv_event_t* e)
    {
        withr::emergency_stop();
    }
}
