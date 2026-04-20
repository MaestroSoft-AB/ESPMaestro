#ifndef __WIFI_HANDLER_H__
#define __WIFI_HANDLER_H__

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define WIFI_MANAGER_MAX_APS 20

typedef struct {
  char ssid[33];
  int8_t rssi;
  bool open;
} Wifi_Handler_ap;

typedef void (*wifi_handler_scan_cb)(const Wifi_Handler_ap *_aps,
                                     uint16_t _count);
typedef void (*wifi_handler_status_cb)(bool _connected, const char *_ssid,
                                       const char *_ip, const char *_message);

esp_err_t wifi_handler_init(wifi_handler_scan_cb _scan_cb,
                            wifi_handler_status_cb _status_cb);
esp_err_t wifi_handler_scan(void);
esp_err_t wifi_handler_connect(const char *_ssid, const char *_password);
esp_err_t wifi_handler_disconnect(void);
bool wifi_handler_is_connected(void);

#endif
