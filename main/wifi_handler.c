#include "wifi_handler.h"

#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"
#include "nvs_flash.h"

static const char *TAG = "WIFI";

static wifi_handler_scan_cb s_scan_cb = NULL;
static wifi_handler_status_cb s_status_cb = NULL;
static esp_netif_t *s_sta_netif = NULL;

static bool s_connected = false;
static bool s_user_disconnect = false;
static char s_current_ssid[33] = {0};

/**************************************************************/
static void wifi_handler_emit_status(bool _connected, const char *_ssid,
                                     const char *_ip, const char *_msg) {
  if (s_status_cb) {
    s_status_cb(_connected, _ssid, _ip, _msg);
  }
}

static void wifi_handler_event_manager(void *_arg, esp_event_base_t _event_base,
                                       int32_t _event_id, void *_event_data) {
  (void)_arg;

  if (_event_base == WIFI_EVENT) {
    switch (_event_id) {
    case WIFI_EVENT_STA_START:
      ESP_LOGI(TAG, "WiFi STA Started");
      break;

    case WIFI_EVENT_SCAN_DONE: {
      uint16_t count = WIFI_MANAGER_MAX_APS;
      wifi_ap_record_t records[WIFI_MANAGER_MAX_APS];
      memset(records, 0, sizeof(records));

      esp_err_t err = esp_wifi_scan_get_ap_records(&count, records);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_scan_get_ap_records failed: %s",
                 esp_err_to_name(err));
        wifi_handler_emit_status(false, NULL, NULL, "Scan failed");
        return;
      }

      static Wifi_Handler_ap aps[WIFI_MANAGER_MAX_APS];
      memset(aps, 0, sizeof(aps));

      uint16_t out_count = 0;
      for (uint16_t i = 0; i < count; i++) {
        if (records[i].ssid[0] == '\0') {
          // skip hidden ssid's
          continue;
        }
        strncpy(aps[out_count].ssid, (const char *)records[i].ssid,
                sizeof(aps[out_count].ssid) - 1);
        aps[out_count].rssi = records[i].rssi;
        aps[out_count].open = (records[i].authmode == WIFI_AUTH_OPEN);
        out_count++;
      }

      if (s_scan_cb) {
        s_scan_cb(aps, out_count);
      }
      break;
    }
    case WIFI_EVENT_STA_DISCONNECTED: {
      wifi_event_sta_disconnected_t *disc =
          (wifi_event_sta_disconnected_t *)_event_data;

      s_connected = false;

      ESP_LOGW(TAG, "Disconnected, reason: %d", disc ? disc->reason : -1);

      wifi_handler_emit_status(false, s_current_ssid, NULL, "Disconnected");

      if (!s_user_disconnect) {
        ESP_LOGI(TAG, "Reconnecting...");
        // esp_wifi_connect();
      }
      break;
    }
    default:
      break;
    }
  } else if (_event_base == IP_EVENT) {
    switch (_event_id) {
    case IP_EVENT_STA_GOT_IP: {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)_event_data;
      char ip_str[16] = {0};

      snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));
      s_connected = true;

      ESP_LOGI(TAG, "Got IP: %s", ip_str);
      wifi_handler_emit_status(true, s_current_ssid, ip_str, "Connected");
      break;
    }
    default:
      break;
    }
  }
}

esp_err_t wifi_handler_init(wifi_handler_scan_cb _scan_cb,
                            wifi_handler_status_cb _status_cb) {
  s_scan_cb = _scan_cb;
  s_status_cb = _status_cb;

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }

  ESP_ERROR_CHECK(err);

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  s_sta_netif = esp_netif_create_default_wifi_sta();
  if (!s_sta_netif) {
    return ESP_FAIL;
  }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_handler_event_manager, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_handler_event_manager, NULL));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  return ESP_OK;
}

esp_err_t wifi_handler_scan(void) {
  wifi_scan_config_t scan_cfg = {0}; // All channels
  return esp_wifi_scan_start(&scan_cfg, false);
}

esp_err_t wifi_handler_connect(const char *_ssid, const char *_password) {
  if (!_ssid || _ssid[0] == '\0') {
    return ESP_ERR_INVALID_ARG;
  }

  wifi_config_t cfg = {0};

  strncpy((char *)cfg.sta.ssid, _ssid, sizeof(cfg.sta.ssid) - 1);
  if (_password) {
    strncpy((char *)cfg.sta.password, _password, sizeof(cfg.sta.password) - 1);
  }

  // Store for status & reconnect
  strncpy(s_current_ssid, _ssid, sizeof(s_current_ssid) - 1);
  s_user_disconnect = false;

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));

  return esp_wifi_connect();
}

esp_err_t wifi_handler_disconnect(void) {
  s_user_disconnect = true;
  s_connected = false;

  return esp_wifi_disconnect();
}

bool wifi_handler_is_connected(void) { return s_connected; }
