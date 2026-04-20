#include "ui.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "wifi_handler.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

extern const lv_font_t notosans_14;
static const char *TAG = "UI";

/*--------------------Internal Helpers-----------------*/
static void ui_build_root(UI *_UI);
static void ui_build_header(UI *_UI);
static void ui_build_content(UI *_UI);
static void ui_build_footer(UI *_UI);
static void ui_build_screen_home(UI *_UI);

static void ui_build_screen_settings(UI *_UI);
static void ui_build_screen_wifi(UI *_UI);
static void ui_build_screen_facility(UI *_UI);

static lv_obj_t *ui_create_card(lv_obj_t *_parent, const char *_title,
                                const char *_subtitle);
static lv_obj_t *ui_create_textarea(lv_obj_t *_parent,
                                    const char *_placeholder);
static void settings_card_event_cb(lv_event_t *_event);
static void back_btn_event_cb(lv_event_t *_event);
static void settings_btn_event_cb(lv_event_t *_event);
static void connect_event_cb(lv_event_t *_event);
static void fake_save_facility_event_cb(lv_event_t *_event);
static void settings_btn_event_cb(lv_event_t *_event);

static void wifi_scan_btn_event_cb(lv_event_t *_event);
static void wifi_dropdown_event_cb(lv_event_t *_event);
static void wifi_ta_event_cb(lv_event_t *_event);

// static char *ui_get_iso_time_string(void);
/*----------------------------------------------------*/
// static char *ui_get_iso_time_string(void) {
//   static char iso_string[20] = {0};
//
//   time_t epoch = time(NULL);
//   struct tm *tm = gmtime(&epoch);
//
//   if (tm) {
//     int year = tm->tm_year + 1900;
//     int month = tm->tm_mon + 1;
//     int day = tm->tm_mday;
//     int hour = tm->tm_hour;
//     int min = tm->tm_min;
//     int sec = tm->tm_sec;
//
//     if (snprintf(iso_string, sizeof(iso_string),
//                  "%04d-%02d-%02dT%02d:%02d:%02d", year, month, day, hour,
//                  min, sec) < 0) {
//       snprintf(iso_string, sizeof(iso_string), "N/A");
//     }
//   } else {
//     snprintf(iso_string, sizeof(iso_string), "N/A");
//   }
//   return iso_string;
// }

void ui_init(UI *_UI) {
  if (!_UI)
    return;

  memset(_UI, 0, sizeof(UI));
  snprintf(_UI->wifi_status, sizeof(_UI->wifi_status), "WIFI: Not connected");

  ESP_LOGI(TAG, "Initializing UI...");

  ui_build_root(_UI);
  ui_build_header(_UI);
  ui_build_content(_UI);
  ui_build_footer(_UI);

  ui_build_screen_home(_UI);
  ui_build_screen_settings(_UI);
  ui_build_screen_wifi(_UI);
  ui_build_screen_facility(_UI);

  ui_show_screen(_UI, UI_SCREEN_HOME);
}

static void ui_build_root(UI *_UI) {
  if (!_UI)
    return;

  lv_obj_t *screen = lv_scr_act();

  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  // LV_OPA_COVER makes the screen background fully opaque

  _UI->root = lv_obj_create(screen);
  lv_obj_set_size(_UI->root, LV_PCT(100), LV_PCT(100));
  // LV_PCT(100) sizes the object relative to its parent, so root fills the

  lv_obj_center(_UI->root);

  lv_obj_set_style_radius(_UI->root, 0, 0);
  lv_obj_set_style_border_width(_UI->root, 0, 0);
  lv_obj_set_style_pad_all(_UI->root, 0, 0);
  // Remove default styling so layout starts from a clean edge-to-edge container

  lv_obj_set_style_bg_color(_UI->root, lv_color_black(), 0);

  lv_obj_set_layout(_UI->root, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->root, LV_FLEX_FLOW_COLUMN);
  // Stack children vertically: header > body > footer
}

static void ui_build_header(UI *_UI) {
  if (!_UI)
    return;

  _UI->header = lv_obj_create(_UI->root);
  lv_obj_set_width(_UI->header, LV_PCT(100));
  lv_obj_set_height(_UI->header, 70);

  lv_obj_set_style_radius(_UI->header, 0, 0);
  lv_obj_set_style_border_width(_UI->header, 0, 0);
  lv_obj_set_style_pad_left(_UI->header, 12, 0);
  lv_obj_set_style_pad_right(_UI->header, 12, 0);
  lv_obj_set_style_pad_top(_UI->header, 10, 0);
  lv_obj_set_style_pad_bottom(_UI->header, 10, 0);
  lv_obj_set_style_bg_color(_UI->header, lv_color_hex(0x050709), 0);

  lv_obj_set_layout(_UI->header, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(_UI->header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  /*--------------BUTTONS-------------------*/
  _UI->back_btn = lv_btn_create(_UI->header);
  lv_obj_set_size(_UI->back_btn, 44, 44);
  lv_obj_add_event_cb(_UI->back_btn, back_btn_event_cb, LV_EVENT_CLICKED, _UI);

  lv_obj_t *back_label = lv_label_create(_UI->back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_center(back_label);

  /*--------------TITLE-------------------*/
  _UI->header_title = lv_label_create(_UI->header);
  lv_obj_set_style_text_color(_UI->header_title, lv_color_white(), 0);
  lv_obj_set_style_text_font(_UI->header_title, &notosans_14, 0);
  lv_label_set_text(_UI->header_title, "ESPMaestro");

  lv_obj_set_style_pad_left(_UI->header_title, 12, 0);

  lv_obj_t *spacer = lv_obj_create(_UI->header);
  lv_obj_set_flex_grow(spacer, 1);
  lv_obj_set_height(spacer, 1);
  lv_obj_set_style_bg_opa(spacer, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(spacer, 0, 0);

  _UI->settings_btn = lv_btn_create(_UI->header);
  lv_obj_set_size(_UI->settings_btn, 44, 44);
  lv_obj_add_event_cb(_UI->settings_btn, settings_btn_event_cb,
                      LV_EVENT_CLICKED, _UI);

  lv_obj_t *settings_label = lv_label_create(_UI->settings_btn);
  lv_label_set_text(settings_label, LV_SYMBOL_SETTINGS);
  lv_obj_center(settings_label);
}

static void ui_build_content(UI *_UI) {
  if (!_UI)
    return;

  _UI->content = lv_obj_create(_UI->root);
  lv_obj_set_width(_UI->content, LV_PCT(100));
  lv_obj_set_flex_grow(_UI->content, 1);

  lv_obj_set_style_radius(_UI->content, 0, 0);
  lv_obj_set_style_border_width(_UI->content, 0, 0);
  lv_obj_set_style_pad_all(_UI->content, 16, 0);
  lv_obj_set_style_bg_color(_UI->content, lv_color_hex(0x111315), 0);
}

static void ui_build_footer(UI *_UI) {
  if (!_UI)
    return;

  _UI->footer = lv_obj_create(_UI->root);
  lv_obj_set_width(_UI->footer, LV_PCT(100));
  lv_obj_set_height(_UI->footer, 36);

  lv_obj_set_style_radius(_UI->footer, 0, 0);
  lv_obj_set_style_border_width(_UI->footer, 0, 0);
  lv_obj_set_style_bg_color(_UI->footer, lv_color_hex(0x101820), 0);
  lv_obj_set_style_pad_left(_UI->footer, 12, 0);
  lv_obj_set_style_pad_right(_UI->footer, 8, 0);

  _UI->footer_label = lv_label_create(_UI->footer);
  lv_obj_set_style_text_color(_UI->footer_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(_UI->footer_label, &notosans_14, 0);
  lv_label_set_text(_UI->footer_label, "FAKTA");
}

static lv_obj_t *ui_create_card(lv_obj_t *_parent, const char *_title,
                                const char *_subtitle) {
  lv_obj_t *card = lv_btn_create(_parent);
  lv_obj_set_width(card, LV_PCT(100));
  lv_obj_set_height(card, 90);

  lv_obj_set_style_bg_color(card, lv_color_hex(0x050709), 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_radius(card, 16, 0);

  lv_obj_set_layout(card, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(card, 14, 0);
  lv_obj_set_style_pad_gap(card, 6, 0);

  lv_obj_t *title_label = lv_label_create(card);
  lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(title_label, &notosans_14, 0);
  lv_label_set_text(title_label, _title);

  lv_obj_t *subtitle_label = lv_label_create(card);
  lv_obj_set_style_text_color(subtitle_label, lv_color_hex(0x909090), 0);
  lv_obj_set_style_text_font(subtitle_label, &notosans_14, 0);
  lv_label_set_text(subtitle_label, _subtitle);

  return card;
}

static void ui_build_screen_settings(UI *_UI) {
  _UI->screen_settings = lv_obj_create(_UI->content);

  lv_obj_set_size(_UI->screen_settings, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(_UI->screen_settings, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_UI->screen_settings, 0, 0);
  lv_obj_set_style_pad_all(_UI->screen_settings, 0, 0);

  lv_obj_set_layout(_UI->screen_settings, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->screen_settings, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(_UI->screen_settings, 16, 0);

  _UI->card_wifi = ui_create_card(_UI->screen_settings, "WiFi Configuration",
                                  "Configure network settings");

  _UI->card_facility =
      ui_create_card(_UI->screen_settings, "Facility Configuration",
                     "Configure facility details");

  lv_obj_add_event_cb(_UI->card_wifi, settings_card_event_cb, LV_EVENT_CLICKED,
                      _UI);
  lv_obj_add_event_cb(_UI->card_facility, settings_card_event_cb,
                      LV_EVENT_CLICKED, _UI);
}

static lv_obj_t *ui_create_textarea(lv_obj_t *_parent,
                                    const char *_placeholder) {
  lv_obj_t *ta = lv_textarea_create(_parent);
  lv_obj_set_width(ta, LV_PCT(100));
  lv_obj_set_height(ta, 50);

  lv_textarea_set_placeholder_text(ta, _placeholder);
  lv_obj_set_style_bg_color(ta, lv_color_hex(0x050709), 0);
  lv_obj_set_style_text_color(ta, lv_color_white(), 0);
  lv_obj_set_style_border_width(ta, 0, 0);
  lv_obj_set_style_radius(ta, 12, 0);

  return ta;
}
static void ui_build_screen_home(UI *_UI) {
  if (!_UI)
    return;

  _UI->screen_home = lv_obj_create(_UI->content);
  lv_obj_set_size(_UI->screen_home, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(_UI->screen_home, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_UI->screen_home, 0, 0);
  lv_obj_set_style_pad_all(_UI->screen_home, 0, 0);

  lv_obj_set_layout(_UI->screen_home, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->screen_home, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(_UI->screen_home, 16, 0);

  lv_obj_t *title = lv_label_create(_UI->screen_home);
  lv_label_set_text(title, "ESPMaestro");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_set_style_text_font(title, &notosans_14, 0);

  lv_obj_t *chart_card = lv_obj_create(_UI->screen_home);
  lv_obj_set_width(chart_card, LV_PCT(100));
  lv_obj_set_height(chart_card, LV_PCT(70));
  lv_obj_set_style_radius(chart_card, 16, 0);
  lv_obj_set_style_border_width(chart_card, 0, 0);
  lv_obj_set_style_bg_color(chart_card, lv_color_hex(0x050709), 0);
  lv_obj_set_style_pad_all(chart_card, 12, 0);

  lv_obj_t *chart = lv_chart_create(chart_card);
  lv_obj_set_size(chart, LV_PCT(100), LV_PCT(100));
  lv_obj_center(chart);

  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart, 12);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);

  lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, 0);
  lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);

  lv_chart_series_t *ser = lv_chart_add_series(
      chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

  static int32_t values[] = {25, 40, 35, 60, 55, 70, 65, 80, 72, 68, 75, 90};
  for (int i = 0; i < 12; i++) {
    lv_chart_set_next_value(chart, ser, values[i]);
  }

  lv_chart_refresh(chart);

  lv_obj_t *status = lv_label_create(_UI->screen_home);
  lv_label_set_text(status, "System OK");
  lv_obj_set_style_text_color(status, lv_color_hex(0xA0A0A0), 0);
}

static void ui_build_screen_wifi(UI *_UI) {
  if (!_UI)
    return;

  _UI->screen_wifi = lv_obj_create(_UI->content);
  lv_obj_set_size(_UI->screen_wifi, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(_UI->screen_wifi, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_UI->screen_wifi, 0, 0);
  lv_obj_set_style_pad_all(_UI->screen_wifi, 0, 0);

  lv_obj_set_layout(_UI->screen_wifi, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->screen_wifi, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(_UI->screen_wifi, 12, 0);

  lv_obj_t *ssid_label = lv_label_create(_UI->screen_wifi);
  lv_label_set_text(ssid_label, "Network Name (SSID)");
  lv_obj_set_style_text_color(ssid_label, lv_color_white(), 0);

  _UI->wifi_networks_dd = lv_dropdown_create(_UI->screen_wifi);
  lv_obj_set_width(_UI->wifi_networks_dd, LV_PCT(100));
  lv_dropdown_set_options(_UI->wifi_networks_dd, "Scan for networks...");
  lv_obj_add_event_cb(_UI->wifi_networks_dd, wifi_dropdown_event_cb,
                      LV_EVENT_VALUE_CHANGED, _UI);

  _UI->wifi_scan_btn = lv_btn_create(_UI->screen_wifi);
  lv_obj_set_width(_UI->wifi_scan_btn, LV_PCT(100));
  lv_obj_set_height(_UI->wifi_scan_btn, 50);
  lv_obj_add_event_cb(_UI->wifi_scan_btn, wifi_scan_btn_event_cb,
                      LV_EVENT_CLICKED, _UI);

  lv_obj_t *scan_label = lv_label_create(_UI->wifi_scan_btn);
  lv_label_set_text(scan_label, "Scan networks");
  lv_obj_center(scan_label);

  _UI->wifi_ssid_ta =
      ui_create_textarea(_UI->screen_wifi, "Enter WiFi network name");
  lv_textarea_set_one_line(_UI->wifi_ssid_ta, true);
  lv_obj_add_event_cb(_UI->wifi_ssid_ta, wifi_ta_event_cb, LV_EVENT_FOCUSED,
                      _UI);

  lv_obj_t *pass_label = lv_label_create(_UI->screen_wifi);
  lv_label_set_text(pass_label, "Password");
  lv_obj_set_style_text_color(pass_label, lv_color_white(), 0);

  _UI->wifi_pass_ta =
      ui_create_textarea(_UI->screen_wifi, "Enter WiFi password");
  lv_textarea_set_one_line(_UI->wifi_pass_ta, true);
  lv_textarea_set_password_mode(_UI->wifi_pass_ta, true);
  lv_obj_add_event_cb(_UI->wifi_pass_ta, wifi_ta_event_cb, LV_EVENT_FOCUSED,
                      _UI);

  _UI->wifi_connect_btn = lv_btn_create(_UI->screen_wifi);
  lv_obj_set_width(_UI->wifi_connect_btn, LV_PCT(100));
  lv_obj_set_height(_UI->wifi_connect_btn, 50);
  lv_obj_add_event_cb(_UI->wifi_connect_btn, connect_event_cb, LV_EVENT_CLICKED,
                      _UI);

  lv_obj_t *btn_label = lv_label_create(_UI->wifi_connect_btn);
  lv_label_set_text(btn_label, "Connect");
  lv_obj_center(btn_label);

  _UI->wifi_status_label = lv_label_create(_UI->screen_wifi);
  lv_label_set_text(_UI->wifi_status_label, "");
  lv_obj_set_style_text_color(_UI->wifi_status_label, lv_color_white(), 0);

  _UI->wifi_keyboard = lv_keyboard_create(_UI->screen_wifi);
  lv_obj_set_width(_UI->wifi_keyboard, LV_PCT(100));
  lv_obj_add_flag(_UI->wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void ui_build_screen_facility(UI *_UI) {
  if (!_UI)
    return;

  _UI->screen_facility = lv_obj_create(_UI->content);
  lv_obj_set_size(_UI->screen_facility, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(_UI->screen_facility, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(_UI->screen_facility, 0, 0);
  lv_obj_set_style_pad_all(_UI->screen_facility, 0, 0);

  lv_obj_set_layout(_UI->screen_facility, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->screen_facility, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(_UI->screen_facility, 12, 0);

  _UI->facility_name_ta =
      ui_create_textarea(_UI->screen_facility, "Facility name");
  _UI->facility_country_ta =
      ui_create_textarea(_UI->screen_facility, "Country");
  _UI->facility_address_ta =
      ui_create_textarea(_UI->screen_facility, "Address");
  _UI->facility_city_ta = ui_create_textarea(_UI->screen_facility, "City");

  _UI->facility_save_btn = lv_btn_create(_UI->screen_facility);
  lv_obj_set_width(_UI->facility_save_btn, LV_PCT(100));
  lv_obj_set_height(_UI->facility_save_btn, 50);
  lv_obj_add_event_cb(_UI->facility_save_btn, fake_save_facility_event_cb,
                      LV_EVENT_CLICKED, _UI);

  lv_obj_t *btn_label = lv_label_create(_UI->facility_save_btn);
  lv_label_set_text(btn_label, "Save");
  lv_obj_center(btn_label);

  _UI->facility_status_label = lv_label_create(_UI->screen_facility);
  lv_label_set_text(_UI->facility_status_label, "");
  lv_obj_set_style_text_color(_UI->facility_status_label, lv_color_white(), 0);
}

void ui_show_screen(UI *_UI, UI_Screen _screen) {
  if (!_UI)
    return;
  _UI->current_screen = _screen;

  if (_UI->screen_home)
    lv_obj_add_flag(_UI->screen_home, LV_OBJ_FLAG_HIDDEN);
  if (_UI->screen_settings)
    lv_obj_add_flag(_UI->screen_settings, LV_OBJ_FLAG_HIDDEN);
  if (_UI->screen_wifi)
    lv_obj_add_flag(_UI->screen_wifi, LV_OBJ_FLAG_HIDDEN);
  if (_UI->screen_facility)
    lv_obj_add_flag(_UI->screen_facility, LV_OBJ_FLAG_HIDDEN);

  switch (_screen) {
  case UI_SCREEN_HOME:
    lv_obj_clear_flag(_UI->screen_home, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_UI->header_title, "ESPMaestro");
    lv_label_set_text(_UI->footer_label, "Overview");
    break;

  case UI_SCREEN_SETTINGS:
    lv_obj_clear_flag(_UI->screen_settings, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_UI->header_title, "Settings");
    lv_label_set_text(_UI->footer_label, "Choose a section");
    break;

  case UI_SCREEN_WIFI:
    lv_obj_clear_flag(_UI->screen_wifi, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_UI->header_title, "WiFi Configuration");
    lv_label_set_text(_UI->footer_label, "Enter credentials");
    break;

  case UI_SCREEN_FACILITY:
    lv_obj_clear_flag(_UI->screen_facility, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_UI->header_title, "Facility Configuration");
    lv_label_set_text(_UI->footer_label, "Edit facility settings");
    break;
  }

  // Remove back btn from homescreen
  if (_screen == UI_SCREEN_HOME) {
    lv_obj_add_flag(_UI->back_btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(_UI->back_btn, LV_OBJ_FLAG_HIDDEN);
  }
}

static void connect_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  if (!_UI)
    return;

  const char *ssid = lv_textarea_get_text(_UI->wifi_ssid_ta);
  const char *pass = lv_textarea_get_text(_UI->wifi_pass_ta);

  if (!ssid || strlen(ssid) == 0) {
    ui_set_wifi_form_status(_UI, "Please enter SSID", true);
    return;
  }

  ui_set_wifi_busy(_UI, true);
  ui_set_wifi_form_status(_UI, "Connecting...", false);

  esp_err_t err = wifi_handler_connect(ssid, pass);
  if (err != ESP_OK) {
    ui_set_wifi_busy(_UI, false);
    ui_set_wifi_form_status(_UI, "Failed to start connection", true);
  }
}

static void settings_btn_event_cb(lv_event_t *_event) {
  if (!_event)
    return;

  UI *_UI = (UI *)lv_event_get_user_data(_event);
  if (!_UI)
    return;

  ui_show_screen(_UI, UI_SCREEN_SETTINGS);
}

static void fake_save_facility_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  if (!_UI || !_UI->facility_status_label)
    return;

  lv_label_set_text(_UI->facility_status_label, "Saved fakta");
}

void ui_set_wifi_form_status(UI *_UI, const char *_msg, bool _error) {
  if (!_UI || !_UI->wifi_status_label)
    return;

  const char *new_text = _msg ? _msg : "";
  const char *current = lv_label_get_text(_UI->wifi_status_label);

  if (!current || strcmp(current, new_text) != 0) {
    lv_label_set_text(_UI->wifi_status_label, new_text);
  }

  if (_error) {
    lv_obj_set_style_text_color(_UI->wifi_status_label,
                                lv_palette_main(LV_PALETTE_RED), 0);
  } else {
    lv_obj_set_style_text_color(_UI->wifi_status_label, lv_color_white(), 0);
  }
}

void ui_set_wifi_busy(UI *_UI, bool _busy) {
  if (!_UI || !_UI->wifi_connect_btn)
    return;

  _UI->wifi_connecting = _busy;

  if (_busy) {
    lv_obj_add_state(_UI->wifi_connect_btn, LV_STATE_DISABLED);
    ui_set_wifi_form_status(_UI, "Connecting...", false);
  } else {
    lv_obj_clear_state(_UI->wifi_connect_btn, LV_STATE_DISABLED);
  }
}

void ui_set_footer_text(UI *_UI, const char *_text) {
  if (!_UI || !_UI->footer_label || !_text)
    return;

  const char *current = lv_label_get_text(_UI->footer_label);
  if (current && strcmp(current, _text) == 0) {
    return; // ingen förändring -> ingen redraw
  }

  lv_label_set_text(_UI->footer_label, _text);
}

void ui_set_wifi_status(UI *_UI, bool _connected, const char *_ssid,
                        const char *_ip) {
  if (!_UI)
    return;

  if (_connected && _ssid && _ip) {
    snprintf(_UI->wifi_status, sizeof(_UI->wifi_status),
             "WiFi OK | SSID: %s | IP: %s", _ssid, _ip);
  } else {
    snprintf(_UI->wifi_status, sizeof(_UI->wifi_status), "WiFi: Not connected");
  }

  if (_UI->footer_label) {
    ui_set_footer_text(_UI, _UI->wifi_status);
  }
}

static void settings_card_event_cb(lv_event_t *_event) {
  if (!_event)
    return;

  UI *_UI = (UI *)lv_event_get_user_data(_event);
  lv_obj_t *target = lv_event_get_target(_event);

  if (!_UI || !target)
    return;

  if (target == _UI->card_wifi) {
    ui_show_screen(_UI, UI_SCREEN_WIFI);
  } else if (target == _UI->card_facility) {
    ui_show_screen(_UI, UI_SCREEN_FACILITY);
  }
}

static void back_btn_event_cb(lv_event_t *_event) {
  if (!_event)
    return;

  UI *_UI = (UI *)lv_event_get_user_data(_event);
  if (!_UI)
    return;

  switch (_UI->current_screen) {
  case UI_SCREEN_WIFI:
  case UI_SCREEN_FACILITY:
    ui_show_screen(_UI, UI_SCREEN_SETTINGS);
    break;

  case UI_SCREEN_SETTINGS:
    ui_show_screen(_UI, UI_SCREEN_HOME);
    break;

  case UI_SCREEN_HOME:
  default:
    break;
  }
}

void ui_set_wifi_network_list(UI *_UI, const char *_options) {
  if (!_UI || !_UI->wifi_networks_dd || !_options)
    return;
  lv_dropdown_set_options(_UI->wifi_networks_dd, _options);
}

static void wifi_dropdown_event_cb(lv_event_t *_event) {
  UI *_UI = (UI *)lv_event_get_user_data(_event);

  if (!_UI || !_UI->wifi_networks_dd || !_UI->wifi_ssid_ta)
    return;

  char ssid[64] = {0};
  lv_dropdown_get_selected_str(_UI->wifi_networks_dd, ssid, sizeof(ssid));

  if (strcmp(ssid, "Scan for networks...") != 0 &&
      strcmp(ssid, "No networks found") != 0) {
    lv_textarea_set_text(_UI->wifi_ssid_ta, ssid);
  }
}

static void wifi_ta_event_cb(lv_event_t *_event) {
  UI *_UI = (UI *)lv_event_get_user_data(_event);
  lv_obj_t *ta = lv_event_get_target(_event);

  if (!_UI || !ta || !_UI->wifi_keyboard)
    return;

  lv_keyboard_set_textarea(_UI->wifi_keyboard, ta);
  lv_obj_clear_flag(_UI->wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_scan_btn_event_cb(lv_event_t *_event) {
  UI *_UI = (UI *)lv_event_get_user_data(_event);
  if (!_UI)
    return;

  ui_set_wifi_form_status(_UI, "Scanning...", false);

  if (wifi_handler_scan() != ESP_OK) {
    ui_set_wifi_form_status(_UI, "Failed to start scan", true);
  }
}

void ui_tick(UI *_UI) { (void)_UI; }
