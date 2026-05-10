#include "withrottle_client.h"

#include "WiThrottleProtocol.h"
#include "SocketStream.h"
#include "wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char* TAG = "withr_client";

// ---- Configuration --------------------------------------------------------

#define WITHR_HOST "192.168.1.7" // Replace with actual WiThrottle server IP
#define WITHR_PORT 12090
#define WITHR_LOCO_ADDR "S3" // 'S' = short address, '3' = DCC address 3
#define WITHR_DEVICE "withrottle_dial"
#define WITHR_TASK_STACK (8 * 1024) // WiThrottleProtocol::inputbuffer is 32KB alone
#define WITHR_TASK_PRIO 4           // Below LVGL (5), above UI update (2-3)

// make static since its input buffer is MASSIVE (32kb)
static WiThrottleProtocol wiThrottle;

// ---- Command queue --------------------------------------------------------

typedef struct
{
    enum
    {
        CMD_SPEED,
        CMD_DIR,
        CMD_ESTOP
    } type;
    int value; // speed 0-126, or 1=Forward / 0=Reverse for CMD_DIR
} withr_cmd_t;

static QueueHandle_t s_cmd_queue = nullptr;
static volatile bool s_connected = false;

// ---- Delegate -------------------------------------------------------------

class ThrottleDelegate : public WiThrottleProtocolDelegate
{
public:
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
        ESP_LOGI(TAG, "Loco acquired: %s", address.c_str());
        s_connected = true;
    }
    void addressRemoved(String address, String command) override
    {
        ESP_LOGI(TAG, "Loco released: %s", address.c_str());
        s_connected = false;
    }
};

// ---- Task -----------------------------------------------------------------

static void withrottle_task(void* arg)
{
    ThrottleDelegate delegate;
    SocketStream     client;

    wiThrottle.setDelegate(&delegate);

    while (true)
    {
        // 1. Wait for WiFi
        while (!is_wifi_connected())
        {
            ESP_LOGI(TAG, "Waiting for WiFi...");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // 2. Connect TCP socket
        ESP_LOGI(TAG, "Connecting to %s:%d", WITHR_HOST, WITHR_PORT);
        if (!client.connect(WITHR_HOST, WITHR_PORT))
        {
            ESP_LOGE(TAG, "TCP connect failed, retrying in 5s");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // 3. Initialize WiThrottle session
        wiThrottle.connect(&client);
        wiThrottle.setDeviceName(WITHR_DEVICE);
        wiThrottle.addLocomotive(DEFAULT_MULTITHROTTLE, WITHR_LOCO_ADDR);
        ESP_LOGI(TAG, "WiThrottle session started");

        // 4. Main loop
        while (client.connected())
        {
            // Drain command queue from UI task
            withr_cmd_t cmd;
            while (xQueueReceive(s_cmd_queue, &cmd, 0) == pdTRUE)
            {
                switch (cmd.type)
                {
                    case withr_cmd_t::CMD_SPEED:
                        wiThrottle.setSpeed(DEFAULT_MULTITHROTTLE, cmd.value);
                        break;
                    case withr_cmd_t::CMD_DIR:
                        wiThrottle.setDirection(DEFAULT_MULTITHROTTLE,
                                                cmd.value ? Forward : Reverse);
                        break;
                    case withr_cmd_t::CMD_ESTOP:
                        wiThrottle.emergencyStop();
                        break;
                }
            }

            // Parse incoming server data and send queued outbound commands
            // (check() also handles heartbeat internally)
            wiThrottle.check();

            vTaskDelay(pdMS_TO_TICKS(50));
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

void set_speed(int speed)
{
    withr_cmd_t cmd = {withr_cmd_t::CMD_SPEED, speed};
    xQueueSend(s_cmd_queue, &cmd, 0); // non-blocking; drop if queue full
}

void emergency_stop(void)
{
    withr_cmd_t cmd = {withr_cmd_t::CMD_ESTOP, 0};
    xQueueSend(s_cmd_queue, &cmd, 0);
}

void set_direction(bool fwd)
{
    withr_cmd_t cmd = {withr_cmd_t::CMD_DIR, fwd ? 1 : 0};
    xQueueSend(s_cmd_queue, &cmd, 0);
}

bool is_connected(void)
{
    return s_connected;
}

} // namespace withr
