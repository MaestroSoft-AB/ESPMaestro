/******************** ESPMaestro ********************/
/* Copyright MaestroSoft Corp AB Inc LLC Unlimited. */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wifi_handler.h"

#include "display_handler.h"

static const char* TAG = "main";

void app_main(void)
{
 /*
  if (display_handler_init(NULL) != 0) {
    ESP_LOGE(TAG, "Failed to init display_handler");
  }
  */
  
  wh_start(NULL);

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
  //xTaskCreate(display_handler_work, "display_handler_work", 8192, NULL, 5, NULL);
	// xTaskCreate(wh_start, "wh_start", 8192, NULL, 4, NULL);
}
