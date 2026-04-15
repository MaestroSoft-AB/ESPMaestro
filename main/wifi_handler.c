#include "wifi_handler.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <stdint.h>
#include <string.h>

#include <esp_netif.h>
#include <nvs_flash.h>

static const char *TAG = "WIFI";

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
              .ssid = "REDACTED_SSID",
              .password = "REDACTED_PASSWORD",
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

  while (1) {

    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
      ESP_LOGI(TAG, "Ansluten till: %s", ap_info.ssid);
    } else {
      ESP_LOGI(TAG, "Inte ansluten");
    }

    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
      ESP_LOGI(TAG, "Nuvarande IP: " IPSTR, IP2STR(&ip_info.ip));
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void wh_dispose() {}
