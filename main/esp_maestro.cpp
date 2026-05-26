/******************** ESPMaestro ********************/
/* Copyright MaestroSoft Corp AB Inc LLC Unlimited. */

#include "display_handler.h"
#include "esp_log.h"
#include "facility_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "scheduler.h"
#include "ui_status.hpp"
#include "wifi_handler.h"
#include <stdio.h>
#include <string.h>
static const char *TAG = "main";

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

  if (display_handler_init(NULL) != 0) {
    ESP_LOGE(TAG, "Failed to init display_handler");
  } else {

    /*  Start display worker task only on init success */
    if (xTaskCreate(display_handler_work, "display_handler_work", 12288, NULL,
                    3, NULL) != pdPASS) {
      ESP_LOGE(TAG, "Failed to create display_handler_work task");
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
