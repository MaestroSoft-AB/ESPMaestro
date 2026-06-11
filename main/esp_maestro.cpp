/******************** ESPMaestro ********************/
/* Copyright MaestroSoft Corp AB Inc LLC Unlimited. */
#include "bme280_sensor.hpp"
extern "C" {
#include "cli.h"
#include "i2c.h"
}
#include "dashboard_data.hpp"
#include "display_handler.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "facility_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "scheduler.h"
#include "ui_status.hpp"
#include "wifi_handler.h"
#define MAESTROUTILS_WITH_CJSON 1
#include <cJSON.h>

static const char *TAG = "main";
static constexpr uint32_t DISPLAY_HANDLER_TASK_STACK = 16384;

static DH display_context = {};

#ifndef OPTIMAESTRO_DISPLAY_CURRENT_URL
#define OPTIMAESTRO_DISPLAY_CURRENT_URL                                        \
  "http://135.225.131.246:10580/api/v1/display/current"
#endif

typedef struct {
  char *data;
  int len;
  int capacity;
} LiveHttpBuffer;

static esp_err_t live_power_http_event_handler(esp_http_client_event_t *evt) {
  if (evt->event_id != HTTP_EVENT_ON_DATA || evt->data == NULL ||
      evt->data_len <= 0) {
    return ESP_OK;
  }

  LiveHttpBuffer *buffer = static_cast<LiveHttpBuffer *>(evt->user_data);
  if (!buffer || !buffer->data)
    return ESP_FAIL;

  if (buffer->len + evt->data_len >= buffer->capacity) {
    return ESP_FAIL;
  }

  memcpy(buffer->data + buffer->len, evt->data, evt->data_len);
  buffer->len += evt->data_len;
  buffer->data[buffer->len] = '\0';
  return ESP_OK;
}

static bool fetch_live_power(uint32_t *power_w_out) {
  if (!power_w_out || !wifi_handler_is_connected())
    return false;

  char response[768] = {0};
  LiveHttpBuffer buffer = {
      .data = response,
      .len = 0,
      .capacity = (int)sizeof(response),
  };

  esp_http_client_config_t config = {};
  config.url = OPTIMAESTRO_DISPLAY_CURRENT_URL;
  config.timeout_ms = 5000;
  config.event_handler = live_power_http_event_handler;
  config.user_data = &buffer;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client)
    return false;

  esp_err_t err = esp_http_client_perform(client);
  int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);

  if (err != ESP_OK || status != 200 || buffer.len <= 0)
    return false;

  cJSON *root = cJSON_Parse(response);
  if (!root)
    return false;

  cJSON *power = cJSON_GetObjectItemCaseSensitive(root, "power_w");
  bool ok = cJSON_IsNumber(power) && power->valuedouble >= 0.0;
  if (ok) {
    *power_w_out = (uint32_t)(power->valuedouble + 0.5);
  }
  cJSON_Delete(root);
  return ok;
}

static bme280 sensor;

static void live_power_task(void *arg) {
  (void)arg;

  while (true) {
    uint32_t power_w = 0;
    if (fetch_live_power(&power_w)) {
      display_handler_update_live_power(power_w);
    }

    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

extern "C" void app_main(void) {

  esp_log_level_set("*", ESP_LOG_INFO);

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }

  ESP_ERROR_CHECK(err);
  facility_config_init();

  /* Initialize display first */

  display_context.i2c = DEV_I2C_Init();

  if (display_context.i2c.bus == NULL) {
    ESP_LOGE(TAG, "Failed to init shared I2C bus");
    return;
  }

  if (display_handler_init(&display_context) != 0) {
    ESP_LOGE(TAG, "Failed to init display_handler");
  } else {

    /*  Start display worker task only on init success */
    if (xTaskCreate(display_handler_work, "display_handler_work",
                    DISPLAY_HANDLER_TASK_STACK, NULL,
                    3, NULL) != pdPASS) {
      ESP_LOGE(TAG, "Failed to create display_handler_work task");
    }

    sensor.start(display_context.i2c.bus, 2000);

    if (scheduler_init() != 0) {
      ESP_LOGE(TAG, "Failed to init scheduler");
    } else {
      if (xTaskCreate(scheduler_task, "scheduler_task", 4096, NULL, 1, NULL) !=
          pdPASS) {
        ESP_LOGE(TAG, "Failed to create scheduler_task");
      }
    }
    if (xTaskCreate(live_power_task, "live_power_task", 6144, NULL, 1, NULL) !=
        pdPASS) {
      ESP_LOGE(TAG, "Failed to create live_power_task");
    }
  }

  if (wifi_handler_init(on_wifi_scan_done, on_wifi_status) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init wifi manager");
    return;
  }

  cli_init();

  bool missing_wifi = !wifi_handler_has_saved_config();
  bool missing_facility = !facility_config_is_configured();
  if (missing_wifi && missing_facility) {
    display_handler_start_setup_wizard(true, true);
  } else if (missing_wifi) {
    display_handler_set_footer_text("Warning: Wifi config missing");
  } else if (missing_facility) {
    display_handler_set_footer_text("Warning: Facility config missing");
  }

  static DashboardData dashboard_data;
  static UiStatus uistatus(&sensor);
}
