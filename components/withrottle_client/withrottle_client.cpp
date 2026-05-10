#include "withrottle_client.h"

#include "WiThrottleProtocol.h"
#include "SocketStream.h"
#include "wifi.h"
#include <map>
#include "Param.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char* TAG = "withr_client";

// ---- Configuration --------------------------------------------------------

params::Param<std::string> s_withr_ip{"withr_ip", std::string("192.168.1.7")};
params::Param<int>         s_withr_port{"withr_port", 12090};

#define WITHR_LOCO_ADDR "S3" // 'S' = short address, '3' = DCC address 3
#define WITHR_DEVICE "withrottle_dial"
#define WITHR_TASK_STACK (8 * 1024) // WiThrottleProtocol::inputbuffer is 32KB alone
#define WITHR_TASK_PRIO 4           // Below LVGL (5), above UI update (2-3)

// make static since its input buffer is MASSIVE (32kb)
static WiThrottleProtocol s_wiThrottle;

// ---- Command queue --------------------------------------------------------

typedef struct
{
    enum
    {
        CMD_SPEED,
        CMD_DIR,
        CMD_ESTOP
    } type;
    int  value; // speed 0-126, or 1=Forward / 0=Reverse for CMD_DIR
    char throttle = DEFAULT_MULTITHROTTLE;
} withr_cmd_t;

static QueueHandle_t s_cmd_queue = nullptr;
static volatile bool s_connected = false;

static uint8_t MAX_THROTTLE_VALUE = 126;

// ---- Helpers! -------------------------------------------------------------

static constexpr uint8_t percent_to_speed(uint8_t percent)
{
    return (percent * MAX_THROTTLE_VALUE) / 100;
}

static constexpr uint8_t speed_to_percent(uint8_t speed)
{
    return (speed * 100) / MAX_THROTTLE_VALUE;
}

// ---- Delegate -------------------------------------------------------------

class ThrottleDelegate : public WiThrottleProtocolDelegate
{
public:
    // types:

    struct RosterInfo
    {
        /// human name for the RosterEntry
        std::string name;
        char        length; // 'S' or 'L'
        /// Set to true if this Roster entry has been added to the throttle, otherwise false
        bool on_throttle = false;
    };

    // methods:
    void receivedVersion(String version) override
    {
        ESP_LOGI(TAG, "Server version: %s", version.c_str());
    }

    void receivedTrackPower(TrackPower state) override
    {
        ESP_LOGI(TAG, "Track power: %d", (int) state);
    }

    void addressAdded(String address, String entry) override
    {
        if (address.length() < 2)
        {
            return; // bad format
        }
        char addr_len = address[0];
        int  addr_num = std::stoi(address.substring(1).c_str());
        ESP_LOGI(TAG, "Loco acquired: %s", address.c_str());
        for (auto& [key, info] : roster)
        {
            if (key == addr_num && info.length == addr_len)
            {
                info.on_throttle = true;
                break;
            }
        }
        s_connected = true;
    }

    void addressRemoved(String address, String command) override
    {
        if (address.length() >= 2)
        {
            char addr_len = address[0];
            int  addr_num = std::stoi(address.substring(1).c_str());
            for (auto& [key, info] : roster)
            {
                if (key == addr_num && info.length == addr_len)
                {
                    info.on_throttle = false;
                    break;
                }
            }
        }
        ESP_LOGI(TAG, "Loco released: %s", address.c_str());
        s_connected = false;
    }

    void receivedDirection(Direction dir) override
    {
        current_direction = dir;
    }

    void receivedRosterEntry(int index, String name, int address, char length) override
    {
        roster[address] = RosterInfo{name.c_str(), length};
        ESP_LOGD(TAG, "Roster[%d]: '%s' (%c%d)", index, name.c_str(), length, address);
    }

    // Cached data:

    Direction current_direction = Direction::Forward;
    /// Roster cache: key = DCC address integer, value = RosterInfo
    std::map<int, RosterInfo> roster;
};

ThrottleDelegate s_delegate;

// ---- Task -----------------------------------------------------------------

static void withrottle_task(void* arg)
{
    SocketStream client;

    s_wiThrottle.setDelegate(&s_delegate);

    while (true)
    {
        // 1. Wait for WiFi
        while (!is_wifi_connected())
        {
            ESP_LOGI(TAG, "Waiting for WiFi...");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // 2. Connect TCP socket
        std::string withrottle_ip   = s_withr_ip.get();
        int         withrottle_port = s_withr_port.get();
        ESP_LOGI(TAG, "Connecting to %s:%d", withrottle_ip, withrottle_port);
        if (!client.connect(withrottle_ip.c_str(), withrottle_port))
        {
            ESP_LOGE(TAG, "TCP connect failed, retrying in 5s");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // 3. Initialize WiThrottle session
        s_wiThrottle.connect(&client);
        s_wiThrottle.setDeviceName(WITHR_DEVICE);
        s_wiThrottle.addLocomotive(DEFAULT_MULTITHROTTLE, WITHR_LOCO_ADDR);
        ESP_LOGI(TAG, "WiThrottle session started");

        // 4. Main loop
        while (client.connected())
        {
            // Drain command queue from UI task
            withr_cmd_t cmd;
            while (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(50)) == pdTRUE)
            {
                switch (cmd.type)
                {
                    case withr_cmd_t::CMD_SPEED:
                        s_wiThrottle.setSpeed(cmd.throttle, cmd.value);
                        break;
                    case withr_cmd_t::CMD_DIR:
                        s_wiThrottle.setDirection(cmd.throttle, cmd.value ? Forward : Reverse);
                        break;
                    case withr_cmd_t::CMD_ESTOP:
                        ESP_LOGI(TAG, "ESTOP CALLED");
                        s_wiThrottle.emergencyStop(cmd.throttle);
                        break;
                }
            }

            // Parse incoming server data and send queued outbound commands
            // (check() also handles heartbeat internally)
            s_wiThrottle.check();
        }

        // 5. Disconnected — clean up and retry
        ESP_LOGW(TAG, "Disconnected from WiThrottle server, reconnecting in 3s");
        s_connected = false;
        client.disconnect();
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

// ---- Public API -----------------------------------------------------------

namespace withr
{

void client_start(void)
{
    s_cmd_queue = xQueueCreate(8, sizeof(withr_cmd_t));
    BaseType_t result =
        xTaskCreate(withrottle_task, "withr", WITHR_TASK_STACK, nullptr, WITHR_TASK_PRIO, nullptr);
    if (result != pdPASS)
    {
        ESP_LOGE(TAG, "failed to start withrottle client task!");
    }
}

std::optional<uint8_t> get_speed()
{
    if (is_connected())
    {
        int speed = s_wiThrottle.getSpeed(DEFAULT_MULTITHROTTLE);
        if (speed < 0 || speed > MAX_THROTTLE_VALUE)
        {
            return std::nullopt;
        }
        return speed_to_percent(speed);
    }
    return std::nullopt;
}

std::optional<Direction> get_direction()
{
    if (is_connected())
    {
        return s_delegate.current_direction;
    }
    return std::nullopt;
}

void set_speed(uint8_t speed_percent)
{
    if (speed_percent > 100)
    {
        return;
    }
    withr_cmd_t cmd = {withr_cmd_t::CMD_SPEED, percent_to_speed(speed_percent)};
    xQueueSend(s_cmd_queue, &cmd, 0); // non-blocking; drop if queue full
}

void emergency_stop(void)
{
    withr_cmd_t cmd = {withr_cmd_t::CMD_ESTOP, 0};
    xQueueSend(s_cmd_queue, &cmd, 0);
}

void set_direction(Direction dir)
{
    withr_cmd_t cmd = {withr_cmd_t::CMD_DIR, dir == Direction::Forward ? 1 : 0};
    xQueueSend(s_cmd_queue, &cmd, 0);
}

bool is_connected(void)
{
    return s_connected;
}

std::optional<std::string> get_loco_name()
{
    for (const auto& [key, value] : s_delegate.roster)
    {
        if (value.on_throttle)
        {
            return value.name;
        }
    }
    return std::nullopt;
}

std::string get_server_url()
{
    std::string withrottle_ip   = s_withr_ip.get();
    int         withrottle_port = s_withr_port.get();
    return withrottle_ip + ":" + std::to_string(withrottle_port);
}

} // namespace withr
