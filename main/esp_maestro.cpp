/******************** ESPMaestro ********************/
/* Copyright MaestroSoft Corp AB Inc LLC Unlimited. */
#include "bme280_sensor.hpp"
extern "C" {
#include "i2c.h"
}
#include "dashboard_data.hpp"
#include "display_handler.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "facility_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "scheduler.h"
#include "ui_status.hpp"
#include "wifi_handler.h"

static const char *TAG = "main";

#include "bme280_sensor.hpp"
#include "i2c.h"
static DH display_context = {};

static void bme280_test_task(void *arg) {
  DH *ctx = static_cast<DH *>(arg);

  if (ctx == nullptr || ctx->i2c.bus == nullptr) {
    ESP_LOGE("BME280_TEST", "Missing I2C bus");
    vTaskDelete(NULL);
    return;
  }

  bme280 sensor;

  if (!sensor.init(ctx->i2c.bus)) {
    ESP_LOGE("BME280_TEST", "Failed to init BME280");
    vTaskDelete(NULL);
    return;
  }

  while (true) {
    if (sensor.read()) {
      bme280_reading reading = {};

      if (sensor.latest(&reading)) {
        ESP_LOGI("BME280_TEST", "T=%.2f C | RH=%.2f %% | P=%.2f hPa",
                 reading.temperature_c, reading.humidity_rh,
                 reading.pressure_hpa);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

extern "C" void app_main(void) {

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
    if (xTaskCreate(display_handler_work, "display_handler_work", 12288, NULL,
                    3, NULL) != pdPASS) {
      ESP_LOGE(TAG, "Failed to create display_handler_work task");
    }
    if (xTaskCreate(bme280_test_task, "bme280_test_task", 4096,
                    &display_context, 1, NULL) != pdPASS) {
      ESP_LOGE(TAG, "Failed to create bme280_test_task");
    }
  }

  if (scheduler_init() != 0) {
    ESP_LOGE(TAG, "Failed to init scheduler");
  } else {
    if (xTaskCreate(scheduler_task, "scheduler_task", 4096, NULL, 1, NULL) !=
        pdPASS) {
      ESP_LOGE(TAG, "Failed to create scheduler_task");
    }
  }

  if (wifi_handler_init(on_wifi_scan_done, on_wifi_status) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init wifi manager");
    return;
  }

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
  static UiStatus uistatus;
}

/*
- `TaskFunction_t pxTaskCode (aka void (*)(void *))`
- `const char *const pcName`
- `const uint32_t usStackDepth (aka const unsigned int)`
- `void *const pvParameters`
- `UBaseType_t uxPriority (aka unsigned int)`
- `TaskHandle_t *const pxCreatedTask (aka struct tskTaskControlBlock **const)`
*/

/* Can use parameters to send inited struct with backend info
 * And callback to use with xTaskNotify to do backend stuff */
// xTaskCreate(display_handler_work, "display_handler_work", 8192, NULL, 5,
// NULL);
//  xTaskCreate(wh_start, "wh_start", 8192, NULL, 4, NULL);
