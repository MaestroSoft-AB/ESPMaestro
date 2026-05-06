#include "wifi_handler.h"

#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

/**
 * @file wifi_handler.c
 * @brief ESP-IDF WiFi handler implementation.
 *
 * Implements a singleton WiFi state machine around ESP-IDF station mode.
 * Credentials are stored by the ESP-IDF WiFi driver using NVS flash storage.
 */

static const char *TAG = "WIFI";

/**
 * @brief Singleton runtime state for the WiFi handler.
 *
 * ESP32-S3 has one WiFi station interface, so this module uses one internal
 * instance instead of exposing multiple handler objects.
 */

static Wifi_Handler s_wifi = {
    .state = WIFI_HANDLER_STATE_IDLE,
};

esp_err_t wifi_handler_finish_connect(void);
/**
 * @brief Convert a WiFi handler state to a readable string.
 *
 * Used for logging state transitions.
 */
static const char *wifi_handler_state_name(wifi_handler_state state) {
  switch (state) {
  case WIFI_HANDLER_STATE_IDLE:
    return "IDLE";
  case WIFI_HANDLER_STATE_SCANNING:
    return "SCANNING";
  case WIFI_HANDLER_STATE_CONNECTING:
    return "CONNECTING";
  case WIFI_HANDLER_STATE_CONNECTED:
    return "CONNECTED";
  case WIFI_HANDLER_STATE_RECONNECT_WAIT:
    return "RECONNECT_WAIT";
  case WIFI_HANDLER_STATE_SWITCHING:
    return "SWITCHING";
  case WIFI_HANDLER_STATE_ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

/**
 * @brief Update the internal state and log the transition.
 *
 * This should be the only place where s_wifi.state is modified directly.
 */
static void wifi_handler_set_state(wifi_handler_state new_state) {
  if (s_wifi.state == new_state) {
    return;
  }

  ESP_LOGI(TAG, "State: %s -> %s", wifi_handler_state_name(s_wifi.state),
           wifi_handler_state_name(new_state));

  s_wifi.state = new_state;
}

/**
 * @brief Emit a WiFi status update to the registered application callback.
 *
 * Safe to call even when no callback has been registered.
 */
static void wifi_handler_emit_status(bool _connected, const char *_ssid,
                                     const char *_ip, const char *_msg) {
  if (s_wifi.status_cb) {
    s_wifi.status_cb(_connected, _ssid, _ip, _msg);
  }
}

/**
 * @brief Handle ESP-IDF WiFi and IP events.
 *
 * This function drives the WiFi handler state machine:
 * - WIFI_EVENT_SCAN_DONE collects scan results and resumes connection if
 * needed.
 * - WIFI_EVENT_STA_DISCONNECTED handles user disconnects and reconnect
 * attempts.
 * - IP_EVENT_STA_GOT_IP marks the handler as connected.
 *
 * @param _arg        Unused user argument.
 * @param _event_base ESP-IDF event base.
 * @param _event_id   ESP-IDF event identifier.
 * @param _event_data Event-specific payload.
 */
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

      if (s_wifi.scan_cb) {
        s_wifi.scan_cb(aps, out_count);
      }

      wifi_handler_set_state(WIFI_HANDLER_STATE_IDLE);

      if (!s_wifi.user_disconnect && s_wifi.current_ssid[0] != '\0') {
        ESP_LOGI(TAG, "Scan done, reconnecting to saved SSID");
        wifi_handler_set_state(WIFI_HANDLER_STATE_CONNECTING);
        esp_wifi_connect();
      }

      break;
    }
    case WIFI_EVENT_STA_DISCONNECTED: {
      wifi_event_sta_disconnected_t *disc =
          (wifi_event_sta_disconnected_t *)_event_data;

      ESP_LOGW(TAG, "Disconnected, reason: %d", disc ? disc->reason : -1);
      //
      // wifi_handler_emit_status(false, s_wififi.current_ssid, NULL,
      //                          "Disconnected");

      if (s_wifi.state == WIFI_HANDLER_STATE_SWITCHING) {
        wifi_handler_finish_connect();
        break;
      }

      if (s_wifi.user_disconnect) {
        wifi_handler_set_state(WIFI_HANDLER_STATE_IDLE);
        break;
      }

      if (s_wifi.state == WIFI_HANDLER_STATE_SCANNING) {
        break;
      }

      if (s_wifi.retry_count < WIFI_MAX_RETRIES) {
        s_wifi.retry_count++;

        ESP_LOGI(TAG, "Reconnecting... attempt %d/%d", s_wifi.retry_count,
                 WIFI_MAX_RETRIES);

        wifi_handler_set_state(WIFI_HANDLER_STATE_CONNECTING);
        esp_wifi_connect();
      } else {
        wifi_handler_set_state(WIFI_HANDLER_STATE_ERROR);
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
      s_wifi.retry_count = 0;
      wifi_handler_set_state(WIFI_HANDLER_STATE_CONNECTED);

      ESP_LOGI(TAG, "Got IP: %s", ip_str);
      // wifi_handler_emit_status(true, s_wifi.current_ssid, ip_str,
      // "Connected");
      break;
    }
    default:
      break;
    }
  }
}

esp_err_t wifi_handler_init(wifi_handler_scan_cb _scan_cb,
                            wifi_handler_status_cb _status_cb) {
  s_wifi.scan_cb = _scan_cb;
  s_wifi.status_cb = _status_cb;
  s_wifi.user_disconnect = false;
  s_wifi.retry_count = 0;
  wifi_handler_set_state(WIFI_HANDLER_STATE_IDLE);

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }

  ESP_ERROR_CHECK(err);

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  s_wifi.sta_netif = esp_netif_create_default_wifi_sta();
  if (!s_wifi.sta_netif) {
    return ESP_FAIL;
  }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // Store WiFi configuration in flash so credentials persist across reboots.
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));

  ESP_ERROR_CHECK(esp_event_handler_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_handler_event_manager, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_handler_event_manager, NULL));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  wifi_config_t saved_cfg = {0};

  esp_err_t cfg_err = esp_wifi_get_config(WIFI_IF_STA, &saved_cfg);
  if (cfg_err == ESP_OK && saved_cfg.sta.ssid[0] != '\0') {
    s_wifi.cfg = saved_cfg;
    strncpy(s_wifi.current_ssid, (const char *)saved_cfg.sta.ssid,
            sizeof(s_wifi.current_ssid) - 1);

    s_wifi.user_disconnect = false;
    s_wifi.retry_count = 0;
    wifi_handler_set_state(WIFI_HANDLER_STATE_CONNECTING);

    ESP_LOGI(TAG, "Auto-connecting to saved SSID: %s", s_wifi.current_ssid);
    esp_wifi_connect();
  }

  return ESP_OK;
}

esp_err_t wifi_handler_scan(void) {
  if (s_wifi.state == WIFI_HANDLER_STATE_SCANNING) {
    return ESP_ERR_WIFI_STATE;
  }

  // ESP-IDF does not allow scanning while the station is actively connecting.
  // Disconnect first, then reconnect when WIFI_EVENT_SCAN_DONE is received.
  if (s_wifi.state == WIFI_HANDLER_STATE_CONNECTING ||
      s_wifi.state == WIFI_HANDLER_STATE_RECONNECT_WAIT) {
    esp_wifi_disconnect();
  }

  wifi_handler_set_state(WIFI_HANDLER_STATE_SCANNING);

  wifi_scan_config_t scan_cfg = {
      .ssid = NULL,
      .bssid = NULL,
      .channel = 0,
      .show_hidden = false,
  };

  esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);

  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_wifi_scan_start failed: %s", esp_err_to_name(err));

    wifi_handler_set_state(WIFI_HANDLER_STATE_ERROR);

    if (!s_wifi.user_disconnect && s_wifi.current_ssid[0] != '\0') {
      wifi_handler_set_state(WIFI_HANDLER_STATE_CONNECTING);
      esp_wifi_connect();
    }
  }

  return err;
}

esp_err_t wifi_handler_connect(const char *_ssid, const char *_password) {
  if (!_ssid || _ssid[0] == '\0') {
    return ESP_ERR_INVALID_ARG;
  }
  memset(&s_wifi.cfg, 0, sizeof(s_wifi.cfg));

  strncpy((char *)s_wifi.cfg.sta.ssid, _ssid, sizeof(s_wifi.cfg.sta.ssid) - 1);
  if (_password) {
    strncpy((char *)s_wifi.cfg.sta.password, _password,
            sizeof(s_wifi.cfg.sta.password) - 1);
  }
  // Store for status & reconnect
  strncpy(s_wifi.current_ssid, _ssid, sizeof(s_wifi.current_ssid) - 1);
  s_wifi.user_disconnect = false;
  s_wifi.retry_count = 0;
  if (s_wifi.state == WIFI_HANDLER_STATE_CONNECTED) {
    wifi_handler_set_state(WIFI_HANDLER_STATE_SWITCHING);
    esp_wifi_disconnect();
    return ESP_OK;
  }

  wifi_handler_set_state(WIFI_HANDLER_STATE_CONNECTING);
  return wifi_handler_finish_connect();
}

esp_err_t wifi_handler_finish_connect(void) {

  esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &s_wifi.cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(err));
    wifi_handler_set_state(WIFI_HANDLER_STATE_ERROR);
    return err;
  }
  wifi_handler_set_state(WIFI_HANDLER_STATE_CONNECTING);

  err = esp_wifi_connect();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
    wifi_handler_set_state(WIFI_HANDLER_STATE_ERROR);
  }
  return err;
}

esp_err_t wifi_handler_disconnect(void) {
  s_wifi.user_disconnect = true;
  s_wifi.retry_count = 0;

  wifi_handler_set_state(WIFI_HANDLER_STATE_IDLE);

  return esp_wifi_disconnect();
}
bool wifi_handler_is_connected(void) {
  return s_wifi.state == WIFI_HANDLER_STATE_CONNECTED;
}
