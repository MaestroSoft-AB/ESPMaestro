/******************** ESPMaestro ********************/
/* Copyright MaestroSoft Corp AB Inc LLC Unlimited. */

#include "display_handler.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "gpio.h"
#include "gt911.h"
#include "i2c.h"
#include "io_extension.h"
#include "lvgl_port.h"
#include "misc/lv_color.h"
#include "rgb_lcd_port.h"
#include "ui.h"
#include "widgets/lv_label.h"
#include "wifi_handler.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* --------------------------------------------------------------- */
static DH g_dh = {0};
static esp_lcd_panel_io_handle_t touch_io_handle = NULL;
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

static DH_indoor_climate_status g_indoor_climate_status = {0};
static SemaphoreHandle_t g_indoor_climate_mutex = NULL;

static struct {
  bool ready;
  uint32_t power_w;
} g_live_power_status = {0};
static SemaphoreHandle_t g_live_power_mutex = NULL;

/*---------------------------Dashboard--------------------------------------*/
static WeatherData g_weather;
static ElectricityData g_electricity;
static RealtimeData g_realtime;
static bool g_dashboard_ready = false;
static SemaphoreHandle_t g_dashboard_mutex = NULL;

/*---------------------------Setup
 * Wizard--------------------------------------*/
static bool g_setup_ready = false;
static bool g_setup_missing_wifi = false;
static bool g_setup_missing_facility = false;
static SemaphoreHandle_t g_setup_mutex = NULL;

/*---------------------------Footer
 * Messages-----------------------------------*/
static bool g_footer_ready = false;
static char g_footer_text[128] = {0};
static SemaphoreHandle_t g_footer_mutex = NULL;

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

/****************************TOUCH*********************************/
static esp_lcd_touch_handle_t display_handler_touch_init(DEV_I2C_Port *port) {
  if (port == NULL || port->bus == NULL) {
    ESP_LOGE(TAG, "Missing I2C bus for touch init");
    return NULL;
  }

  esp_lcd_panel_io_i2c_config_t tp_io_config =
      ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();

  IO_EXTENSION_Init();
  DEV_GPIO_Mode(EXAMPLE_PIN_NUM_TOUCH_INT, GPIO_MODE_INPUT_OUTPUT);
  IO_EXTENSION_Output(IO_EXTENSION_IO_1, 0);

  vTaskDelay(pdMS_TO_TICKS(100));
  DEV_Digital_Write(EXAMPLE_PIN_NUM_TOUCH_INT, 0);

  vTaskDelay(pdMS_TO_TICKS(100));
  IO_EXTENSION_Output(IO_EXTENSION_IO_1, 1);

  vTaskDelay(pdMS_TO_TICKS(200));

  ESP_LOGI(TAG, "Initialize GT911 I2C panel IO using shared bus");

  esp_err_t err =
      esp_lcd_new_panel_io_i2c(port->bus, &tp_io_config, &touch_io_handle);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_new_panel_io_i2c failed: %s", esp_err_to_name(err));
    return NULL;
  }

  esp_lcd_touch_config_t tp_cfg = {
      .x_max = DISPLAY_SIZE_WIDTH,
      .y_max = DISPLAY_SIZE_HEIGHT,
      .rst_gpio_num = EXAMPLE_PIN_NUM_TOUCH_RST,
      .int_gpio_num = EXAMPLE_PIN_NUM_TOUCH_INT,
      .levels =
          {
              .reset = 0,
              .interrupt = 0,
          },
      .flags =
          {
              .swap_xy = 0,
              .mirror_x = 0,
              .mirror_y = 0,
          },
  };

  esp_lcd_touch_handle_t touch = NULL;

  err = esp_lcd_touch_new_i2c_gt911(touch_io_handle, &tp_cfg, &touch);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_touch_new_i2c_gt911 failed: %s",
             esp_err_to_name(err));
    return NULL;
  }
  return touch;
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

void display_handler_update_indoor_climate(float temperature_c,
                                           float pressure_hpa,
                                           float humidity_rh) {
  if (!g_indoor_climate_mutex)
    return;

  if (xSemaphoreTake(g_indoor_climate_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    g_indoor_climate_status.temperature_c = temperature_c;
    g_indoor_climate_status.pressure_hpa = pressure_hpa;
    g_indoor_climate_status.humidity_rh = humidity_rh;
    g_indoor_climate_status.indoor_climate_ready = true;
    xSemaphoreGive(g_indoor_climate_mutex);
  }
}

void display_handler_update_live_power(uint32_t power_w) {
  if (!g_live_power_mutex)
    return;

  if (xSemaphoreTake(g_live_power_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    g_live_power_status.power_w = power_w;
    g_live_power_status.ready = true;
    xSemaphoreGive(g_live_power_mutex);
  }
}

/******************************************************************/

void display_handler_update_dashboard(const WeatherData *w,
                                      const ElectricityData *e,
                                      const RealtimeData *r) {
  if (!g_dashboard_mutex)
    return;

  if (xSemaphoreTake(g_dashboard_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    if (w)
      g_weather = *w;
    if (e)
      g_electricity = *e;
    if (r)
      g_realtime = *r;
    g_dashboard_ready = true;
    xSemaphoreGive(g_dashboard_mutex);
  }
}

void display_handler_start_setup_wizard(bool missing_wifi,
                                        bool missing_facility) {
  if (!g_setup_mutex)
    return;

  if (xSemaphoreTake(g_setup_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    g_setup_missing_wifi = missing_wifi;
    g_setup_missing_facility = missing_facility;
    g_setup_ready = missing_wifi || missing_facility;
    xSemaphoreGive(g_setup_mutex);
  }
}

void display_handler_set_footer_text(const char *text) {
  if (!g_footer_mutex || !text)
    return;

  if (xSemaphoreTake(g_footer_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    snprintf(g_footer_text, sizeof(g_footer_text), "%s", text);
    g_footer_ready = true;
    xSemaphoreGive(g_footer_mutex);
  }
}

/*****************************************************************/
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
  if (_DH == NULL || _DH->i2c.bus == NULL) {
    ESP_LOGE(TAG, "display_handler_init missing shared I2C bus");
    return -1;
  }

  g_dh = *_DH;

  tp_handle = display_handler_touch_init(&g_dh.i2c);
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
  if (!g_date_status_mutex) {
    ESP_LOGE(TAG, "Failed to create date status mutex");
    return -1;
  }

  g_indoor_climate_mutex = xSemaphoreCreateMutex();
  if (!g_indoor_climate_mutex) {
    ESP_LOGE(TAG, "Failed to create indoor climate mutex");
    return -1;
  }

  g_live_power_mutex = xSemaphoreCreateMutex();
  if (!g_live_power_mutex) {
    ESP_LOGE(TAG, "Failed to create live power mutex");
    return -1;
  }

  g_dashboard_mutex = xSemaphoreCreateMutex();
  if (!g_dashboard_mutex) {
    ESP_LOGE(TAG, "Failed to create dashboard mutex");
    return -1;
  }

  g_setup_mutex = xSemaphoreCreateMutex();
  if (!g_setup_mutex) {
    ESP_LOGE(TAG, "Failed to create setup mutex");
    return -1;
  }

  g_footer_mutex = xSemaphoreCreateMutex();
  if (!g_footer_mutex) {
    ESP_LOGE(TAG, "Failed to create footer mutex");
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
    bool need_indoor_climate_update = false;
    bool need_live_power_update = false;
    bool need_dashboard_update = false;
    bool need_setup_update = false;
    bool setup_missing_wifi = false;
    bool setup_missing_facility = false;
    bool need_footer_update = false;
    char footer_text[128] = {0};

    WeatherData w;
    ElectricityData e;
    RealtimeData r;
    float indoor_temperature_c = 0.0f;
    float indoor_pressure_hpa = 0.0f;
    float indoor_humidity_rh = 0.0f;
    uint32_t live_power_w = 0;

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

      if (g_indoor_climate_mutex &&
          xSemaphoreTake(g_indoor_climate_mutex, 0) == pdTRUE) {
        if (g_indoor_climate_status.indoor_climate_ready) {
          indoor_temperature_c = g_indoor_climate_status.temperature_c;
          indoor_pressure_hpa = g_indoor_climate_status.pressure_hpa;
          indoor_humidity_rh = g_indoor_climate_status.humidity_rh;
          g_indoor_climate_status.indoor_climate_ready = false;
          need_indoor_climate_update = true;
        }
        xSemaphoreGive(g_indoor_climate_mutex);
      }

      if (need_indoor_climate_update) {
        ui_set_indoor_climate(&g_ui, indoor_temperature_c, indoor_pressure_hpa,
                              indoor_humidity_rh);
      }

      if (g_live_power_mutex && xSemaphoreTake(g_live_power_mutex, 0) == pdTRUE) {
        if (g_live_power_status.ready) {
          live_power_w = g_live_power_status.power_w;
          g_live_power_status.ready = false;
          need_live_power_update = true;
        }
        xSemaphoreGive(g_live_power_mutex);
      }

      if (need_live_power_update) {
        ui_set_live_power(&g_ui, live_power_w);
      }

      if (g_dashboard_mutex && xSemaphoreTake(g_dashboard_mutex, 0) == pdTRUE) {
        if (g_dashboard_ready) {
          w = g_weather;
          e = g_electricity;
          r = g_realtime;
          g_dashboard_ready = false;
          need_dashboard_update = true;
        }
        xSemaphoreGive(g_dashboard_mutex);
      }

      if (need_dashboard_update) {
        ui_set_dashboard_data(&g_ui, &w, &e, &r);
      }

      if (g_setup_mutex && xSemaphoreTake(g_setup_mutex, 0) == pdTRUE) {
        if (g_setup_ready) {
          setup_missing_wifi = g_setup_missing_wifi;
          setup_missing_facility = g_setup_missing_facility;
          g_setup_ready = false;
          need_setup_update = true;
        }
        xSemaphoreGive(g_setup_mutex);
      }

      if (need_setup_update) {
        ui_start_setup_wizard(&g_ui, setup_missing_wifi,
                              setup_missing_facility);
      }

      if (g_footer_mutex && xSemaphoreTake(g_footer_mutex, 0) == pdTRUE) {
        if (g_footer_ready) {
          snprintf(footer_text, sizeof(footer_text), "%s", g_footer_text);
          g_footer_ready = false;
          need_footer_update = true;
        }
        xSemaphoreGive(g_footer_mutex);
      }

      if (need_footer_update) {
        ui_set_footer_text(&g_ui, footer_text);
      }

      /* Perf overlay tick */
      // perf_overlay_tick();

      lvgl_port_unlock();
    }

    vTaskDelayUntil(&x_last_wake, x_freq);
  }
}
