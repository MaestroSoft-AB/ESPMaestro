/******************** ESPMaestro ********************/
/* Copyright MaestroSoft Corp AB Inc LLC Unlimited. */

#include "display_handler.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "gt911.h"
#include "lvgl_port.h"
#include "misc/lv_color.h"
#include "rgb_lcd_port.h"
#include "text_contents.h"
#include "ui.h"
#include "widgets/lv_label.h"
#include "wifi_handler.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* --------------------------------------------------------------- */
static UI g_ui;
static const char *TAG = "display_handler";

extern const lv_font_t notosans_14;

static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_touch_handle_t tp_handle = NULL;

/* Text buffers */
static char screen_text[DISPLAY_MAX_CHAR_ROWS * DISPLAY_MAX_CHAR_PER_ROW] = {0};
static char model_info[87] = {0};
static char iso_string[20] = {0};
static char mem_info[91] = {0};

/* -------------------------------WIFI CALLBACKS----------------------- */
static DH_wifi_status g_wifi_status = {0};
static SemaphoreHandle_t g_wifi_status_mutex = NULL;

/*---------------------------Time & Date--------------------------------------*/
static DH_time_status g_time_status = {0};
static SemaphoreHandle_t g_time_status_mutex = NULL;

static DH_date_status g_date_status = {0};
static SemaphoreHandle_t g_date_status_mutex = NULL;

/* -------------------------------PERF OVERLAY------------------------- */
static lv_obj_t *g_perf_label = NULL;
static uint32_t g_perf_frame_count = 0;
static uint32_t g_perf_last_report_ms = 0;

static void perf_overlay_init(void) {
  if (g_perf_label) {
    return;
  }

  lv_obj_t *screen = lv_scr_act();
  g_perf_label = lv_label_create(screen);

  lv_obj_set_style_bg_opa(g_perf_label, LV_OPA_70, 0);
  lv_obj_set_style_bg_color(g_perf_label, lv_color_black(), 0);
  lv_obj_set_style_text_color(g_perf_label, lv_color_white(), 0);
  lv_obj_set_style_border_width(g_perf_label, 0, 0);
  lv_obj_set_style_shadow_width(g_perf_label, 0, 0);
  lv_obj_set_style_outline_width(g_perf_label, 0, 0);
  lv_obj_set_style_radius(g_perf_label, 4, 0);
  lv_obj_set_style_pad_left(g_perf_label, 6, 0);
  lv_obj_set_style_pad_right(g_perf_label, 6, 0);
  lv_obj_set_style_pad_top(g_perf_label, 4, 0);
  lv_obj_set_style_pad_bottom(g_perf_label, 4, 0);

  lv_obj_align(g_perf_label, LV_ALIGN_TOP_RIGHT, -6, 6);
  lv_label_set_text(g_perf_label, "FPS: --\nframe: -- ms\nheap: --");

  g_perf_last_report_ms = lv_tick_get();
  g_perf_frame_count = 0;
}

static void perf_overlay_tick(void) {
  if (!g_perf_label) {
    return;
  }

  g_perf_frame_count++;

  uint32_t now = lv_tick_get();
  uint32_t elapsed = now - g_perf_last_report_ms;

  if (elapsed >= 1000) {
    uint32_t fps = g_perf_frame_count;
    uint32_t frame_ms = fps ? (1000U / fps) : 0;
    size_t free_heap = esp_get_free_heap_size();

    lv_label_set_text_fmt(g_perf_label, "FPS: %lu\nframe: %lu ms\nheap: %u",
                          (unsigned long)fps, (unsigned long)frame_ms,
                          (unsigned)free_heap);

    g_perf_frame_count = 0;
    g_perf_last_report_ms = now;
  }
}

/* -------------------------------WIFI CALLBACKS----------------------- */
void on_wifi_scan_done(const Wifi_Handler_ap *_aps, uint16_t _count) {
  if (!g_wifi_status_mutex) {
    return;
  }

  if (xSemaphoreTake(g_wifi_status_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return;
  }

  g_wifi_status.scan_options[0] = '\0';

  for (uint16_t i = 0; i < _count; i++) {
    if (_aps[i].ssid[0] == '\0') {
      continue;
    }

    strncat(g_wifi_status.scan_options, _aps[i].ssid,
            sizeof(g_wifi_status.scan_options) -
                strlen(g_wifi_status.scan_options) - 1);

    if (i < _count - 1) {
      strncat(g_wifi_status.scan_options, "\n",
              sizeof(g_wifi_status.scan_options) -
                  strlen(g_wifi_status.scan_options) - 1);
    }
  }

  if (g_wifi_status.scan_options[0] == '\0') {
    strncpy(g_wifi_status.scan_options, "No networks found",
            sizeof(g_wifi_status.scan_options) - 1);
    g_wifi_status.scan_options[sizeof(g_wifi_status.scan_options) - 1] = '\0';
  }

  g_wifi_status.scan_ready = true;

  xSemaphoreGive(g_wifi_status_mutex);
}

void on_wifi_status(bool _connected, const char *_ssid, const char *_ip,
                    const char *_message) {
  if (!g_wifi_status_mutex) {
    return;
  }

  if (xSemaphoreTake(g_wifi_status_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return;
  }

  g_wifi_status.connected = _connected;

  snprintf(g_wifi_status.ssid, sizeof(g_wifi_status.ssid), "%s",
           _ssid ? _ssid : "");
  snprintf(g_wifi_status.ip, sizeof(g_wifi_status.ip), "%s", _ip ? _ip : "");
  snprintf(g_wifi_status.message, sizeof(g_wifi_status.message), "%s",
           _message ? _message : "");

  g_wifi_status.status_ready = true;

  xSemaphoreGive(g_wifi_status_mutex);
}

/******************************************************************/

void display_handler_update_time(uint8_t h, uint8_t m, uint8_t s) {
  if (!g_time_status_mutex)
    return;

  if (xSemaphoreTake(g_time_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    g_time_status.h = h;
    g_time_status.m = m;
    g_time_status.s = s;
    g_time_status.time_ready = true;
    xSemaphoreGive(g_time_status_mutex);
  }
}

void display_handler_update_date(uint16_t year, uint8_t month, uint8_t day) {
  if (!g_date_status_mutex)
    return;

  if (xSemaphoreTake(g_date_status_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    g_date_status.year = year;
    g_date_status.month = month;
    g_date_status.day = day;
    g_date_status.date_ready = true;
    xSemaphoreGive(g_date_status_mutex);
  }
}

/******************************************************************/
static char *get_iso_time_string(void) {
  time_t epoch = time(NULL);
  struct tm *tm = gmtime(&epoch);
  if (tm) {
    int year = tm->tm_year + 1900;
    int month = tm->tm_mon + 1;
    int day = tm->tm_mday;
    int hour = tm->tm_hour;
    int min = tm->tm_min;
    int sec = tm->tm_sec;

    if (snprintf(iso_string, sizeof(iso_string),
                 "%04d-%02d-%02dT%02d:%02d:%02d", year, month, day, hour, min,
                 sec) < 0) {
      ESP_LOGW(TAG, "Failed to parse current time");
      memset(iso_string, 0, sizeof(iso_string));
      snprintf(iso_string, sizeof(iso_string), "N/A");
    }
  } else {
    ESP_LOGW(TAG, "Failed to create tm struct from epoch");
    snprintf(iso_string, sizeof(iso_string), "N/A");
  }

  return iso_string;
}

void display_handler_wifi_status(bool connected, const char *ssid,
                                 const char *ip) {
  if (lvgl_port_lock(-1)) {
    ui_set_wifi_status(&g_ui, connected, ssid, ip);
    lvgl_port_unlock();
  }
}

int display_handler_init(DH *_DH) {
  (void)_DH;

  tp_handle = touch_gt911_init();
  if (tp_handle == NULL) {
    ESP_LOGE(TAG, "Failed to initialize GT911 touch controller");
    return -1;
  }

  panel_handle = waveshare_esp32_s3_rgb_lcd_init();
  if (panel_handle == NULL) {
    ESP_LOGE(TAG, "Failed to initialize RGB LCD panel");
    return -1;
  }

  esp_err_t err = lvgl_port_init(panel_handle, tp_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "lvgl_port_init failed");
    return -1;
  }

  wavesahre_rgb_lcd_bl_on();

  ESP_LOGI(TAG, "Display handler initialized successfully");

  g_wifi_status_mutex = xSemaphoreCreateMutex();
  if (!g_wifi_status_mutex) {
    ESP_LOGE(TAG, "Failed to create wifi status mutex");
    return -1;
  }

  g_time_status_mutex = xSemaphoreCreateMutex();
  if (!g_time_status_mutex) {
    ESP_LOGE(TAG, "Failed to create time status mutex");
    return -1;
  }

  g_date_status_mutex = xSemaphoreCreateMutex();
  if (!g_time_status_mutex) {
    ESP_LOGE(TAG, "Failed to create date status mutex");
    return -1;
  }

  return 0;
}

void display_handler_work(void *_null_for_now) {
  (void)_null_for_now;

  if (lvgl_port_lock(-1)) {
    ui_init(&g_ui);
    ui_set_footer_text(&g_ui, "UI init completed");
    // perf_overlay_init();
    lvgl_port_unlock();
  }

  ESP_LOGI(TAG, "UI initialized, starting loop..");

  TickType_t x_last_wake = xTaskGetTickCount();
  const TickType_t x_freq =
      pdMS_TO_TICKS(33); /* ~30 Hz overlay/update cadence */

  while (1) {
    bool need_ui_update = false;
    bool need_time_update = false;
    bool need_date_update = false;

    if (g_wifi_status_mutex &&
        xSemaphoreTake(g_wifi_status_mutex, 0) == pdTRUE) {
      need_ui_update = g_wifi_status.scan_ready || g_wifi_status.status_ready;
      xSemaphoreGive(g_wifi_status_mutex);
    }

    if (lvgl_port_lock(-1)) {
      if (need_ui_update) {
        if (g_wifi_status_mutex &&
            xSemaphoreTake(g_wifi_status_mutex, 0) == pdTRUE) {

          if (g_wifi_status.scan_ready) {
            ui_set_wifi_network_list(&g_ui, g_wifi_status.scan_options);
            ui_set_wifi_form_status(&g_ui, "Scan complete", false);
            g_wifi_status.scan_ready = false;
          }

          if (g_wifi_status.status_ready) {
            if (g_wifi_status.connected) {
              ui_set_wifi_status(&g_ui, true, g_wifi_status.ssid,
                                 g_wifi_status.ip);
              ui_set_wifi_form_status(&g_ui, "Connected successfully", false);
              ui_set_wifi_busy(&g_ui, false);
            } else {
              ui_set_wifi_form_status(&g_ui,
                                      g_wifi_status.message[0]
                                          ? g_wifi_status.message
                                          : "Disconnected",
                                      true);
              ui_set_wifi_busy(&g_ui, false);
              ui_set_wifi_status(&g_ui, false, NULL, NULL);
            }

            g_wifi_status.status_ready = false;
          }

          xSemaphoreGive(g_wifi_status_mutex);
        }
      }

      if (g_time_status_mutex &&
          xSemaphoreTake(g_time_status_mutex, 0) == pdTRUE) {

        if (g_time_status.time_ready) {
          g_time_status.time_ready = false;
          need_time_update = true;
        }

        xSemaphoreGive(g_time_status_mutex);
      }

      if (need_time_update) {
        ui_set_time(&g_ui, g_time_status.h, g_time_status.m, g_time_status.s);
      }

      if (g_date_status_mutex &&
          xSemaphoreTake(g_date_status_mutex, 0) == pdTRUE) {
        if (g_date_status.date_ready) {
          g_date_status.date_ready = false;
          need_date_update = true;
        }
        xSemaphoreGive(g_date_status_mutex);
      }

      if (need_date_update) {
        ui_set_date(&g_ui, g_date_status.year, g_date_status.month,
                    g_date_status.day);
      }

      /* Perf overlay tick */
      // perf_overlay_tick();

      lvgl_port_unlock();
    }

    vTaskDelayUntil(&x_last_wake, x_freq);
  }
}
