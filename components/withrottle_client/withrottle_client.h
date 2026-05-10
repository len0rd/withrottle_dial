#pragma once
#include <stdbool.h>

namespace withr
{

// Call once from app_main after wifi is initialized.
void client_start(void);

// Thread-safe speed/direction control (call from UI task).
// speed: 0-126
void set_speed(int speed);

constexpr int percent_to_speed(int percent)
{
    return (percent * 126) / 100;
}

// Send an emergency stop.
void emergency_stop(void);

// fwd: true = Forward, false = Reverse
void set_direction(bool fwd);

// Returns true if connected and loco is acquired.
bool is_connected(void);

} // namespace withr
