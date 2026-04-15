#include "display_handler.h"

#include "gt911.h"     // Header for touch screen operations (GT911)
#include "lvgl_port.h" // LVGL porting functions for integration
#include "misc/lv_color.h"
#include "rgb_lcd_port.h" // Header for Waveshare RGB LCD driver
#include "widgets/lv_label.h"
// #include "lv_conf_internal.h"
#include "text_contents.h"

/* TODO: Move out*/
#include "esp_chip_info.h"
#include <time.h>

/* --------------------------------------------------------------- */

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
static char wifi_info[128] = "WIFI: Not connected";

static lv_obj_t *lv_screen = NULL;
// static lv_obj_t* lv_label_menu = NULL;
static lv_obj_t *lv_label_main = NULL;

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
  if (connected && ssid && ip) {
    snprintf(wifi_info, sizeof(wifi_info),
             "WIFI Details\nSSID:: %s\nIP Address: %s", ssid, ip);
  } else {
    snprintf(wifi_info, sizeof(wifi_info), "WIFI: Not connected");
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
  ESP_LOGI(TAG, "display: before touch_gt911_init");
  tp_handle = touch_gt911_init();
  ESP_LOGI(TAG, "display: after touch_gt911_init");
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

static void dh_build_base(void) {
  /* Create full-screen black background */
  lv_screen = lv_scr_act(); // Active screen
  lv_obj_set_style_bg_color(lv_screen, lv_color_black(),
                            LV_PART_MAIN | LV_STATE_DEFAULT);

  /* Add label with ASCII art */
  lv_label_main = lv_label_create(lv_screen);
  lv_obj_align(lv_label_main, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_font(lv_label_main, &notosans_14, 0);
  lv_obj_set_style_text_color(lv_label_main, lv_color_hex(0x32CD32),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void dh_build_logo(void) {
  if (!lv_screen || !lv_label_main) {
    ESP_LOGW(TAG, "No screen or label initialized");
    return;
  }
  lv_label_set_text(lv_label_main, ASCII_ART_MAESTROSOFT_LOGO);
}

/* We wanna make a handler that keeps track of rows/columns better
 * for the strings we want to display in a given space
 * Or just fuck all this shit and use lv_objects properly */
static void dh_build_sysinfo() {
  if (!lv_screen || !lv_label_main) {
    ESP_LOGW(TAG, "No screen or label initialized");
    return;
  }
  lv_label_set_text(lv_label_main, "");

  /* Chip info structs */
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  /* Get model info */
  snprintf(model_info, sizeof(model_info),
           "Model: ESP32-S3 with %i cores\n Features:%s%s%s%s%s\n",
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? " 802.11bgn" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? " BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? " IEEE802154" : "",
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? " SD Flash" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? " Bluetooth" : "");

  char intro[] = "----------------------------------------------------\n"
                 "-------------------- ESPMaestro --------------------\n"
                 "----------------------------------------------------\n";

  /* Get memory info */
  snprintf(mem_info, sizeof(mem_info),
           "Total: %2.2fkB\nInternal: %2.2fkB\nExternal: %2.2fkB\nLargest free "
           "block: %u bytes",
           (float)esp_get_free_heap_size() / 1024,
           (float)heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024,
           (float)heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024,
           heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

  snprintf(screen_text, sizeof(screen_text),
           "%s\nSystem Time: %s\n%s\nMemory %s\n\n%s", intro,
           get_iso_time_string(), model_info, mem_info, wifi_info);

  lv_label_set_text(lv_label_main, screen_text);
}

void display_handler_work(void *_null_for_now) {
  if (lvgl_port_lock(-1)) {
    dh_build_base();
    dh_build_logo();
    ESP_LOGI(TAG, "Do you see display? You should.");
    lvgl_port_unlock();
  }

  /* Wait a bit so we can admire the beautiful logo
   * This could run while other stuff inits like a loading screen */
  vTaskDelay(pdMS_TO_TICKS(5000));
  ESP_LOGI(TAG, "Sysinfo ticks incoming");

  TickType_t x_last_wake = xTaskGetTickCount();
  const TickType_t x_freq = pdMS_TO_TICKS(1000);

  size_t counter = 0;
  while (1) {
    counter++;
    ESP_LOGI(TAG, "System tick #%zu!", counter);

    if (lvgl_port_lock(-1)) {
      dh_build_sysinfo();
      ESP_LOGI(TAG, "Do you see display? You should.");
      lvgl_port_unlock();
    }

    vTaskDelayUntil(&x_last_wake, x_freq);
  }
}
