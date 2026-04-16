#include "wifi_handler.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"

#include "esp_sntp.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <esp_netif.h>
#include <nvs_flash.h>

#include "display_handler.h"

static const char *TAG = "WIFI";

/* Fetch current UTC time using SNTP */
static void fetch_time_utc(void) {
  ESP_LOGI("TIME", "Starting SNTP");

  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");

  esp_sntp_init();

  time_t now = 0;
  struct tm timeinfo = {0};

  int retry = 0;
  const int retry_count = 10;

  while (timeinfo.tm_year < (2020 - 1900) && ++retry < retry_count) {
    ESP_LOGI("TIME", "Setting system time... (%d)", retry);
    vTaskDelay(pdMS_TO_TICKS(2000));
    time(&now);
    localtime_r(&now, &timeinfo);
  }

  if (timeinfo.tm_year >= (2020 - 1900)) {
    ESP_LOGI("TIME", "Time synced");
  } else {
    ESP_LOGW("TIME", "Failed to fetch time");
  }
}

void wh_start(void *args) {
  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  ESP_LOGI(TAG, "Hej");

  esp_err_t err;

  wifi_init_config_t wifiinitcfg = WIFI_INIT_CONFIG_DEFAULT();

  ESP_ERROR_CHECK(err = esp_wifi_init(&wifiinitcfg));
  if (err != ESP_OK) {
    return;
  }

  ESP_LOGI(TAG, "esp_wifi_init err: %d\n", err);

  wifi_config_t wificonf = {
      .sta =
          {
              .ssid = "YourMomDoesntWorkHereBro",
              .password = "TheKingOfTheIronFistTournament",
          },
  };

  ESP_ERROR_CHECK(err = esp_wifi_set_mode(WIFI_MODE_STA));
  if (err != ESP_OK) {
    return;
  }

  ESP_LOGI(TAG, "esp_wifi_mode err: %d\n", err);

  ESP_ERROR_CHECK(err = esp_wifi_set_config(WIFI_IF_STA, &wificonf));
  if (err != ESP_OK) {
    return;
  }

  ESP_LOGI(TAG, "esp_wifi_set_config err: %d\n", err);

  ESP_ERROR_CHECK(err = esp_wifi_start());
  if (err != ESP_OK) {
    return;
  }

  ESP_LOGI(TAG, "esp_wifi_start err: %d\n", err);

  ESP_ERROR_CHECK(err = esp_wifi_connect());
  if (err != ESP_OK) {
    return;
  }

  ESP_LOGI(TAG, "esp_wifi_connect err: %d\n", err);

  esp_netif_ip_info_t ip_info;
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

  wifi_ap_record_t ap_info;

  bool time_synced = false;

  while (1) {

    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
      ESP_LOGI(TAG, "SSID: %s", ap_info.ssid);

      /* Check that netif exists, that we receieve IP info and that it is not 0
       */
      if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        if (ip_info.ip.addr != 0) {

          char ip_str[16];
          /* Convert IPv4 to string using ESP-IDF macros. IPSTR -> "%d.%d.%d.%d"
           * and IP2STR -> extracts the 4 numerical segments from ip_info.ip */
          snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));

          ESP_LOGI(TAG, "IP Address: %s", ip_str);

          display_handler_wifi_status(true, (const char *)ap_info.ssid, ip_str);

          /* Sync time only once after we have a valid IP */
          if (!time_synced) {
            fetch_time_utc();
            time_synced = true;
          }

        } else {
          ESP_LOGI(TAG, "Fetching IP Address...");
          display_handler_wifi_status(true, (const char *)ap_info.ssid,
                                      "Fetching IP Address...");
        }
      } else {
        ESP_LOGI(TAG, "Fetching IP Address...");
        display_handler_wifi_status(true, (const char *)ap_info.ssid,
                                    "Fetching IP Address...");
      }
    } else {
      ESP_LOGI(TAG, "Not connected");
      display_handler_wifi_status(false, NULL, NULL);

      /* Reset time sync if connection drops */
      time_synced = false;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void wh_dispose() {}
