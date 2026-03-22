#ifndef ESP_WIFI_BSP_H
#define ESP_WIFI_BSP_H
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h" //WIFI

void        espwifi_Init(void);
bool        is_wifi_connected();
int         get_wifi_rssi();
std::string get_current_ssid();
std::string get_current_ip_address();

#endif // ESP_WIFI_BSP_H
