/******************** ESPMaestro ********************/
/* Copyright MaestroSoft Corp AB Inc LLC Unlimited. */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "wifi_handler.h"
#include <stdio.h>
#include <string.h>

#include "display_handler.h"

static const char *TAG = "main";

void app_main(void) {

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

  if (wifi_handler_init(on_wifi_scan_done, on_wifi_status) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init wifi manager");
    return;
  }
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
