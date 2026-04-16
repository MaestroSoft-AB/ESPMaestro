#include "display_handler.h"
#include "gt911.h"     // Header for touch screen operations (GT911)
#include "lvgl_port.h" // LVGL porting functions for integration
#include "misc/lv_color.h"
#include "rgb_lcd_port.h" // Header for Waveshare RGB LCD driver
#include "ui.h"
#include "widgets/lv_label.h"
// #include "lv_conf_internal.h"
#include "text_contents.h"

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
/* --------------------------------------------------------------- */

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

  return 0;
}

void display_handler_work(void *_null_for_now) {
  (void)_null_for_now;

  /* Chip info structs */
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  /* Get model info */
  snprintf(mem_info, sizeof(mem_info),
           "Total: %2.2fkB\n"
           "Internal: %2.2fkB\n"
           "External: %2.2fkB\n"
           "Largest free block: %u bytes",
           (float)esp_get_free_heap_size() / 1024,
           (float)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024,
           (float)heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024,
           heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  char intro[] = "----------------------------------------------------\n"
                 "-------------------- ESPMaestro --------------------\n"
                 "----------------------------------------------------\n";
  snprintf(screen_text, sizeof(screen_text),
           "Hello Boss \n%s\nSystem Time: %s\n%s\nMemory %s\n\n Click the "
           "buttons for more FAKTA",
           intro, get_iso_time_string(), model_info, mem_info);

  if (lvgl_port_lock(-1)) {
    ui_init(&g_ui);
    ui_set_body_text(&g_ui, screen_text);
    ui_set_footer_text(&g_ui, "UI init completed");
    lvgl_port_unlock();
  }

  ESP_LOGI(TAG, "UI initialized, starting loop..");
  TickType_t x_last_wake = xTaskGetTickCount();
  const TickType_t x_freq = pdMS_TO_TICKS(1000);
  size_t counter = 0;

  while (1) {
    counter++;
    ESP_LOGI(TAG, "System tick #%zu!", counter);

    if (lvgl_port_lock(-1)) {
      ui_tick(&g_ui);
      lvgl_port_unlock();
    }
    vTaskDelayUntil(&x_last_wake, x_freq);
  }
}
