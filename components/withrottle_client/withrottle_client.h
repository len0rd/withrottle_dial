#pragma once

#include <stdbool.h>
#include <optional>
#include <string>
#include "WiThrottleProtocol.h"

namespace withr
{

// Call once from app_main after wifi is initialized.
void client_start(void);

// Thread-safe speed/direction control (call from UI task).
// speed as a percentage [0,100]
void set_speed(uint8_t percent);

/// @brief Get the current speed of this throttle as a percent [0, 100]
/// nullopt if not currently connected
std::optional<uint8_t> get_speed();

// get throttles current direction
// nullopt if not currently connected
std::optional<Direction> get_direction();

// Send an emergency stop.
void emergency_stop(void);

// set throttle direction
void set_direction(Direction dir);

// Returns true if connected and loco is acquired.
bool is_connected(void);

/// Return the string name of the first locomotive attached to the global throttle
std::optional<std::string> get_loco_name();

/// Get the cached state of function `func` on the throttle loco.
/// Returns nullopt if not connected or func out of range.
std::optional<bool> get_function_state(uint8_t func);

/// Get the name of function `func` on the throttle loco.
/// Returns nullopt if not connected; empty string means function is not defined.
std::optional<std::string> get_function_name(uint8_t func);

/// Send a function on/off command to the throttle loco.
void set_function(uint8_t func, bool state);

/// @brief Get the connection URL Withrottle is using/trying for the Withrottle server
std::string get_server_url();

} // namespace withr
