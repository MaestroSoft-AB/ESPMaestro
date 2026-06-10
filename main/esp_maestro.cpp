/******************** ESPMaestro ********************/
/* Copyright MaestroSoft Corp AB Inc LLC Unlimited. */
#include "bme280_sensor.hpp"
extern "C" {
#include "cli.h"
#include "i2c.h"
}
#include "dashboard_data.hpp"
#include "display_handler.h"
#include "esp_log.h"
#include "facility_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "scheduler.h"
#include "ui_status.hpp"
#include "wifi_handler.h"

static const char *TAG = "main";

static DH display_context = {};
static bme280 sensor;

extern "C" void app_main(void) {

  esp_log_level_set("*", ESP_LOG_ERROR);

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

    sensor.start(display_context.i2c.bus, 2000);

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
}
