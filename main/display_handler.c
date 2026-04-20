#include "display_handler.h"
#include "gt911.h"     // Header for touch screen operations (GT911)
#include "lvgl_port.h" // LVGL porting functions for integration
#include "misc/lv_color.h"
#include "rgb_lcd_port.h" // Header for Waveshare RGB LCD driver
#include "ui.h"
#include "widgets/lv_label.h"
// #include "lv_conf_internal.h"
#include "text_contents.h"
#include "wifi_handler.h"
/* TODO: Move out*/
#include "esp_chip_info.h"
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
// static char* iso_string_offset = NULL;
static char mem_info[91] = {0};
// static char* mem_info_offset = NULL;
/* -------------------------------WIFI CALLBACKS----------------------- */
static DH_wifi_status g_wifi_status = {0};
static SemaphoreHandle_t g_wifi_status_mutex = NULL;

void on_wifi_scan_done(const Wifi_Handler_ap *_aps, uint16_t _count) {
  if (!g_wifi_status_mutex)
    return;

  if (xSemaphoreTake(g_wifi_status_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return;
  }

  g_wifi_status.scan_options[0] = '\0';

  for (uint16_t i = 0; i < _count; i++) {
    if (_aps[i].ssid[0] == '\0')
      continue;

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
  }

  g_wifi_status.scan_ready = true;

  xSemaphoreGive(g_wifi_status_mutex);
}

void on_wifi_status(bool _connected, const char *_ssid, const char *_ip,
                    const char *_message) {
  if (!g_wifi_status_mutex)
    return;

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
/* TODO: Move out */
static char *get_iso_time_string(void) {
  /* Get current time string */
  time_t epoch = time(NULL);
  struct tm *tm = gmtime(&epoch);
  if (tm) {
    int year = tm->tm_year + 1900;
    int month = tm->tm_mon + 1;
    int day = tm->tm_mday;
    int hour = tm->tm_hour;
    int min = tm->tm_min;
    int sec = tm->tm_sec;
    if (snprintf(iso_string, 20, "%04d-%02d-%02dT%02d:%02d:%02d", year, month,
                 day, hour, min, sec) < 0) {
      ESP_LOGW(TAG, "Failed to parse current time");
      memset(iso_string, 0, 20), sprintf(iso_string, "N/A");
    }
  } else {
    ESP_LOGW(TAG, "Failed to create tm struct from epoch");
    sprintf(iso_string, "N/A");
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
  /* _DH is currently unused.
   * Cast to void so the compiler does not warn about it. */
  (void)_DH;

  /* LVGL core must be initialized BEFORE the LVGL port layer.
   * The port layer usually creates/registers display and input devices
   * and assumes LVGL is already ready. */
  lv_init();

  /* Initialize the touch controller first.
   * This should return a valid touch handle, or NULL on failure. */
  tp_handle = touch_gt911_init();
  if (tp_handle == NULL) {
    ESP_LOGE(TAG, "Failed to initialize GT911 touch controller");
    return -1;
  }

  /* Initialize the RGB LCD panel.
   * This should return a valid panel handle, or NULL on failure. */
  panel_handle = waveshare_esp32_s3_rgb_lcd_init();
  if (panel_handle == NULL) {
    ESP_LOGE(TAG, "Failed to initialize RGB LCD panel");
    return -1;
  }

  /* Now that LVGL core is initialized and both hardware handles exist,
   * initialize the LVGL port integration layer. */
  esp_err_t err = lvgl_port_init(panel_handle, tp_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "lvgl_port_init failed");
    return -1;
  }

  /* Turn on the display backlight only after panel init succeeded.
   * Otherwise you may light up a panel that is not actually ready. */
  wavesahre_rgb_lcd_bl_on();

  ESP_LOGI(TAG, "Display handler initialized successfully");

  g_wifi_status_mutex = xSemaphoreCreateMutex();
  if (!g_wifi_status_mutex) {
    ESP_LOGE(TAG, "Failed to create wifi status mutex");
    return -1;
  }

  return 0;
}

void display_handler_work(void *_null_for_now) {
  (void)_null_for_now;

  if (lvgl_port_lock(-1)) {
    ui_init(&g_ui);
    ui_set_footer_text(&g_ui, "UI init completed");
    lvgl_port_unlock();
  }

  ESP_LOGI(TAG, "UI initialized, starting loop..");
  TickType_t x_last_wake = xTaskGetTickCount();
  const TickType_t x_freq = pdMS_TO_TICKS(50);

  while (1) {
    if (lvgl_port_lock(-1)) {
      ui_tick(&g_ui);

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

      lvgl_port_unlock();
    }

    vTaskDelayUntil(&x_last_wake, x_freq);
  }
}
