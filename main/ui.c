#include "ui.h"
#include "esp_log.h"
#include "wifi_handler.h"

#include <stdio.h>
#include <string.h>

extern const lv_font_t notosans_14;
static const char *TAG = "UI";

/*--------------------Internal Helpers-----------------*/
static void ui_build_root(UI *_UI);
static void ui_build_header(UI *_UI);
static void ui_build_content(UI *_UI);
static void ui_build_footer(UI *_UI);

static void ui_destroy_active_screen(UI *_UI);
static void ui_build_active_screen(UI *_UI, UI_Screen _screen);

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
static void wifi_scan_btn_event_cb(lv_event_t *_event);
static void wifi_ta_event_cb(lv_event_t *_event);

static void facility_scroll_up_event_cb(lv_event_t *_event);
static void facility_scroll_down_event_cb(lv_event_t *_event);
static void facility_scroll_by(UI *_UI, int32_t delta);
static void wifi_update_selected_network(UI *_UI);

static void wifi_open_picker_event_cb(lv_event_t *_event);
static void wifi_picker_up_event_cb(lv_event_t *_event);
static void wifi_picker_down_event_cb(lv_event_t *_event);
static void wifi_picker_ok_event_cb(lv_event_t *_event);
static void wifi_picker_cancel_event_cb(lv_event_t *_event);
static void wifi_picker_refresh(UI *_UI);
static void wifi_picker_close(UI *_UI);
/*----------------------------------------------------*/
static lv_obj_t *ui_create_form_field(lv_obj_t *_parent,
                                      const char *_placeholder, lv_coord_t _y,
                                      lv_coord_t _h) {
  lv_obj_t *ta = lv_textarea_create(_parent);
  lv_obj_set_size(ta, LV_PCT(100), _h);
  lv_obj_set_pos(ta, 0, _y);

  lv_textarea_set_placeholder_text(ta, _placeholder);
  lv_obj_set_style_bg_color(ta, lv_color_hex(0x050709), 0);
  lv_obj_set_style_text_color(ta, lv_color_white(), 0);
  lv_obj_set_style_border_width(ta, 0, 0);
  lv_obj_set_style_shadow_width(ta, 0, 0);
  lv_obj_set_style_outline_width(ta, 0, 0);
  lv_obj_set_style_radius(ta, 6, 0);

  lv_obj_set_style_pad_left(ta, 10, 0);
  lv_obj_set_style_pad_right(ta, 10, 0);
  lv_obj_set_style_pad_top(ta, 10, 0);
  lv_obj_set_style_pad_bottom(ta, 10, 0);

  return ta;
}

void ui_init(UI *_UI) {
  if (!_UI)
    return;

  memset(_UI, 0, sizeof(UI));
  snprintf(_UI->wifi_status, sizeof(_UI->wifi_status), "WiFi: Not connected");

  ESP_LOGI(TAG, "Initializing UI...");

  ui_build_root(_UI);
  ui_build_header(_UI);
  ui_build_content(_UI);
  ui_build_footer(_UI);

  ui_show_screen(_UI, UI_SCREEN_HOME);
}

static void ui_build_root(UI *_UI) {
  if (!_UI)
    return;

  lv_obj_t *screen = lv_scr_act();

  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  _UI->root = lv_obj_create(screen);
  lv_obj_set_size(_UI->root, LV_PCT(100), LV_PCT(100));
  lv_obj_center(_UI->root);

  lv_obj_set_style_radius(_UI->root, 0, 0);
  lv_obj_set_style_border_width(_UI->root, 0, 0);
  lv_obj_set_style_pad_all(_UI->root, 0, 0);
  lv_obj_set_style_bg_color(_UI->root, lv_color_black(), 0);
  lv_obj_set_style_shadow_width(_UI->root, 0, 0);
  lv_obj_set_style_outline_width(_UI->root, 0, 0);

  lv_obj_set_layout(_UI->root, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->root, LV_FLEX_FLOW_COLUMN);
}

static void ui_build_header(UI *_UI) {
  if (!_UI)
    return;

  _UI->header = lv_obj_create(_UI->root);
  lv_obj_set_width(_UI->header, LV_PCT(100));
  lv_obj_set_height(_UI->header, 70);

  lv_obj_set_style_radius(_UI->header, 0, 0);
  lv_obj_set_style_border_width(_UI->header, 0, 0);
  lv_obj_set_style_shadow_width(_UI->header, 0, 0);
  lv_obj_set_style_outline_width(_UI->header, 0, 0);
  lv_obj_set_style_pad_left(_UI->header, 12, 0);
  lv_obj_set_style_pad_right(_UI->header, 12, 0);
  lv_obj_set_style_pad_top(_UI->header, 10, 0);
  lv_obj_set_style_pad_bottom(_UI->header, 10, 0);
  lv_obj_set_style_bg_color(_UI->header, lv_color_hex(0x050709), 0);

  lv_obj_set_layout(_UI->header, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(_UI->header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  _UI->back_btn = lv_btn_create(_UI->header);
  lv_obj_set_size(_UI->back_btn, 44, 44);
  lv_obj_set_style_radius(_UI->back_btn, 8, 0);
  lv_obj_set_style_shadow_width(_UI->back_btn, 0, 0);
  lv_obj_set_style_outline_width(_UI->back_btn, 0, 0);
  lv_obj_add_event_cb(_UI->back_btn, back_btn_event_cb, LV_EVENT_CLICKED, _UI);

  lv_obj_t *back_label = lv_label_create(_UI->back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_center(back_label);

  _UI->header_title = lv_label_create(_UI->header);
  lv_obj_set_style_text_color(_UI->header_title, lv_color_white(), 0);
  lv_obj_set_style_text_font(_UI->header_title, &notosans_14, 0);
  lv_label_set_text(_UI->header_title, "ESPMaestro");
  lv_obj_set_style_pad_left(_UI->header_title, 12, 0);

  lv_obj_t *spacer = lv_obj_create(_UI->header);
  lv_obj_set_flex_grow(spacer, 1);
  lv_obj_set_height(spacer, 1);
  lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(spacer, 0, 0);
  lv_obj_set_style_shadow_width(spacer, 0, 0);
  lv_obj_set_style_outline_width(spacer, 0, 0);

  _UI->settings_btn = lv_btn_create(_UI->header);
  lv_obj_set_size(_UI->settings_btn, 44, 44);
  lv_obj_set_style_radius(_UI->settings_btn, 8, 0);
  lv_obj_set_style_shadow_width(_UI->settings_btn, 0, 0);
  lv_obj_set_style_outline_width(_UI->settings_btn, 0, 0);
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
  lv_obj_set_style_shadow_width(_UI->content, 0, 0);
  lv_obj_set_style_outline_width(_UI->content, 0, 0);
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
  lv_obj_set_style_shadow_width(_UI->footer, 0, 0);
  lv_obj_set_style_outline_width(_UI->footer, 0, 0);
  lv_obj_set_style_bg_color(_UI->footer, lv_color_hex(0x101820), 0);
  lv_obj_set_style_pad_left(_UI->footer, 12, 0);
  lv_obj_set_style_pad_right(_UI->footer, 8, 0);

  _UI->footer_label = lv_label_create(_UI->footer);
  lv_obj_set_style_text_color(_UI->footer_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(_UI->footer_label, &notosans_14, 0);
  lv_label_set_text(_UI->footer_label, "FAKTA");
}

static void ui_destroy_active_screen(UI *_UI) {
  if (!_UI || !_UI->active_screen)
    return;

  lv_obj_del(_UI->active_screen);
  _UI->active_screen = NULL;

  _UI->screen_home = NULL;
  _UI->screen_settings = NULL;
  _UI->screen_wifi = NULL;
  _UI->screen_facility = NULL;

  _UI->card_wifi = NULL;
  _UI->card_facility = NULL;

  _UI->wifi_form = NULL;
  _UI->wifi_scan_btn = NULL;
  _UI->wifi_pass_ta = NULL;
  _UI->wifi_connect_btn = NULL;
  _UI->wifi_status_label = NULL;
  _UI->wifi_keyboard = NULL;

  _UI->facility_form = NULL;
  _UI->facility_name_ta = NULL;
  _UI->facility_country_ta = NULL;
  _UI->facility_address_ta = NULL;
  _UI->facility_city_ta = NULL;
  _UI->facility_zip_ta = NULL;
  _UI->facility_state_ta = NULL;
  _UI->facility_contact_ta = NULL;
  _UI->facility_phone_ta = NULL;
  _UI->facility_email_ta = NULL;
  _UI->facility_notes_ta = NULL;
  _UI->facility_save_btn = NULL;
  _UI->facility_status_label = NULL;

  _UI->wifi_ssid_label = NULL;
  _UI->wifi_network_count = 0;
  _UI->wifi_network_selected = 0;
}

static void ui_build_active_screen(UI *_UI, UI_Screen _screen) {
  switch (_screen) {
  case UI_SCREEN_HOME:
    ui_build_screen_home(_UI);
    break;
  case UI_SCREEN_SETTINGS:
    ui_build_screen_settings(_UI);
    break;
  case UI_SCREEN_WIFI:
    ui_build_screen_wifi(_UI);
    break;
  case UI_SCREEN_FACILITY:
    ui_build_screen_facility(_UI);
    break;
  default:
    ui_build_screen_home(_UI);
    break;
  }
}

static lv_obj_t *ui_create_card(lv_obj_t *_parent, const char *_title,
                                const char *_subtitle) {
  lv_obj_t *row = lv_btn_create(_parent);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, 52);

  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_color(row, lv_color_hex(0x333333), 0);
  lv_obj_set_style_radius(row, 0, 0);
  lv_obj_set_style_shadow_width(row, 0, 0);
  lv_obj_set_style_outline_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 4, 0);

  lv_obj_t *label = lv_label_create(row);
  lv_label_set_text_fmt(label, "%s\n%s", _title, _subtitle);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_set_style_text_font(label, &notosans_14, 0);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 4, 0);

  return row;
}

static lv_obj_t *ui_create_textarea(lv_obj_t *_parent,
                                    const char *_placeholder) {
  lv_obj_t *ta = lv_textarea_create(_parent);
  lv_obj_set_width(ta, LV_PCT(100));
  lv_obj_set_height(ta, 46);

  lv_textarea_set_placeholder_text(ta, _placeholder);
  lv_obj_set_style_bg_color(ta, lv_color_hex(0x050709), 0);
  lv_obj_set_style_text_color(ta, lv_color_white(), 0);
  lv_obj_set_style_border_width(ta, 0, 0);
  lv_obj_set_style_shadow_width(ta, 0, 0);
  lv_obj_set_style_outline_width(ta, 0, 0);
  lv_obj_set_style_radius(ta, 6, 0);

  lv_obj_set_style_pad_left(ta, 10, 0);
  lv_obj_set_style_pad_right(ta, 10, 0);
  lv_obj_set_style_pad_top(ta, 10, 0);
  lv_obj_set_style_pad_bottom(ta, 10, 0);

  return ta;
}

static void ui_build_screen_home(UI *_UI) {
  if (!_UI)
    return;

  _UI->screen_home = lv_obj_create(_UI->content);
  _UI->active_screen = _UI->screen_home;

  lv_obj_set_size(_UI->screen_home, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(_UI->screen_home, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(_UI->screen_home, 0, 0);
  lv_obj_set_style_shadow_width(_UI->screen_home, 0, 0);
  lv_obj_set_style_outline_width(_UI->screen_home, 0, 0);
  lv_obj_set_style_pad_all(_UI->screen_home, 0, 0);

  lv_obj_set_layout(_UI->screen_home, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->screen_home, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(_UI->screen_home, 12, 0);

  lv_obj_t *title = lv_label_create(_UI->screen_home);
  lv_label_set_text(title, "ESPMaestro");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_set_style_text_font(title, &notosans_14, 0);

  /* Lättare hemvy än chart för bättre respons */
  lv_obj_t *card1 = ui_create_card(_UI->screen_home, "System",
                                   "Display and touch initialized");
  (void)card1;

  lv_obj_t *card2 = ui_create_card(_UI->screen_home, "Network",
                                   "Open settings to configure WiFi");
  (void)card2;

  lv_obj_t *status = lv_label_create(_UI->screen_home);
  lv_label_set_text(status, "System OK");
  lv_obj_set_style_text_color(status, lv_color_hex(0xA0A0A0), 0);
}

static void ui_build_screen_wifi(UI *_UI) {
  if (!_UI)
    return;

  _UI->screen_wifi = lv_obj_create(_UI->content);
  _UI->active_screen = _UI->screen_wifi;

  lv_obj_set_size(_UI->screen_wifi, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(_UI->screen_wifi, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(_UI->screen_wifi, 0, 0);
  lv_obj_set_style_shadow_width(_UI->screen_wifi, 0, 0);
  lv_obj_set_style_outline_width(_UI->screen_wifi, 0, 0);
  lv_obj_set_style_pad_all(_UI->screen_wifi, 0, 0);

  lv_obj_set_layout(_UI->screen_wifi, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->screen_wifi, LV_FLEX_FLOW_COLUMN);

  _UI->wifi_form = lv_obj_create(_UI->screen_wifi);
  lv_obj_set_width(_UI->wifi_form, LV_PCT(100));
  lv_obj_set_flex_grow(_UI->wifi_form, 1);

  lv_obj_set_style_bg_opa(_UI->wifi_form, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(_UI->wifi_form, 0, 0);
  lv_obj_set_style_shadow_width(_UI->wifi_form, 0, 0);
  lv_obj_set_style_outline_width(_UI->wifi_form, 0, 0);
  lv_obj_set_style_pad_left(_UI->wifi_form, 12, 0);
  lv_obj_set_style_pad_right(_UI->wifi_form, 12, 0);
  lv_obj_set_style_pad_top(_UI->wifi_form, 0, 0);
  lv_obj_set_style_pad_bottom(_UI->wifi_form, 0, 0);
  lv_obj_set_style_pad_gap(_UI->wifi_form, 10, 0);
  lv_obj_set_style_radius(_UI->wifi_form, 0, 0);

  lv_obj_set_layout(_UI->wifi_form, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->wifi_form, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(_UI->wifi_form, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *network_title = lv_label_create(_UI->wifi_form);
  lv_label_set_text(network_title, "Network");
  lv_obj_set_style_text_color(network_title, lv_color_white(), 0);

  lv_obj_t *ssid_row = lv_obj_create(_UI->wifi_form);
  lv_obj_set_width(ssid_row, LV_PCT(100));
  lv_obj_set_height(ssid_row, 58);
  lv_obj_set_style_bg_color(ssid_row, lv_color_hex(0x050709), 0);
  lv_obj_set_style_border_width(ssid_row, 1, 0);
  lv_obj_set_style_border_color(ssid_row, lv_color_hex(0x333333), 0);
  lv_obj_set_style_radius(ssid_row, 8, 0);
  lv_obj_set_style_shadow_width(ssid_row, 0, 0);
  lv_obj_set_style_outline_width(ssid_row, 0, 0);
  lv_obj_set_style_pad_all(ssid_row, 6, 0);
  lv_obj_clear_flag(ssid_row, LV_OBJ_FLAG_SCROLLABLE);

  _UI->wifi_ssid_label = lv_label_create(ssid_row);
  lv_label_set_text(_UI->wifi_ssid_label, "No network selected");
  lv_obj_set_style_text_color(_UI->wifi_ssid_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(_UI->wifi_ssid_label, &notosans_14, 0);
  lv_label_set_long_mode(_UI->wifi_ssid_label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(_UI->wifi_ssid_label, LV_PCT(72));
  lv_obj_align(_UI->wifi_ssid_label, LV_ALIGN_LEFT_MID, 8, 0);

  _UI->wifi_scan_btn = lv_btn_create(_UI->wifi_form);
  lv_obj_set_width(_UI->wifi_scan_btn, LV_PCT(100));
  lv_obj_set_height(_UI->wifi_scan_btn, 42);
  lv_obj_set_style_radius(_UI->wifi_scan_btn, 8, 0);
  lv_obj_set_style_shadow_width(_UI->wifi_scan_btn, 0, 0);
  lv_obj_set_style_outline_width(_UI->wifi_scan_btn, 0, 0);
  lv_obj_add_event_cb(_UI->wifi_scan_btn, wifi_scan_btn_event_cb,
                      LV_EVENT_CLICKED, _UI);

  lv_obj_t *scan_label = lv_label_create(_UI->wifi_scan_btn);
  lv_label_set_text(scan_label, "Scan networks");
  lv_obj_center(scan_label);

  lv_obj_t *pass_label = lv_label_create(_UI->wifi_form);
  lv_label_set_text(pass_label, "Password");
  lv_obj_set_style_text_color(pass_label, lv_color_white(), 0);

  _UI->wifi_pass_ta = ui_create_textarea(_UI->wifi_form, "Enter WiFi password");
  lv_textarea_set_one_line(_UI->wifi_pass_ta, true);
  lv_textarea_set_password_mode(_UI->wifi_pass_ta, true);
  lv_obj_add_event_cb(_UI->wifi_pass_ta, wifi_ta_event_cb, LV_EVENT_FOCUSED,
                      _UI);
  lv_obj_add_event_cb(_UI->wifi_pass_ta, wifi_ta_event_cb, LV_EVENT_DEFOCUSED,
                      _UI);

  _UI->wifi_connect_btn = lv_btn_create(_UI->wifi_form);
  lv_obj_set_width(_UI->wifi_connect_btn, LV_PCT(100));
  lv_obj_set_height(_UI->wifi_connect_btn, 46);
  lv_obj_set_style_radius(_UI->wifi_connect_btn, 8, 0);
  lv_obj_set_style_shadow_width(_UI->wifi_connect_btn, 0, 0);
  lv_obj_set_style_outline_width(_UI->wifi_connect_btn, 0, 0);
  lv_obj_add_event_cb(_UI->wifi_connect_btn, connect_event_cb, LV_EVENT_CLICKED,
                      _UI);

  lv_obj_t *btn_label = lv_label_create(_UI->wifi_connect_btn);
  lv_label_set_text(btn_label, "Connect");
  lv_obj_center(btn_label);

  _UI->wifi_status_label = lv_label_create(_UI->wifi_form);
  lv_label_set_text(_UI->wifi_status_label, "");
  lv_obj_set_width(_UI->wifi_status_label, LV_PCT(100));
  lv_label_set_long_mode(_UI->wifi_status_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(_UI->wifi_status_label, lv_color_white(), 0);

  _UI->wifi_keyboard = lv_keyboard_create(_UI->screen_wifi);
  lv_obj_set_width(_UI->wifi_keyboard, LV_PCT(100));
  lv_obj_set_height(_UI->wifi_keyboard, 160);
  lv_obj_set_style_shadow_width(_UI->wifi_keyboard, 0, 0);
  lv_obj_set_style_outline_width(_UI->wifi_keyboard, 0, 0);
  lv_obj_add_flag(_UI->wifi_keyboard, LV_OBJ_FLAG_HIDDEN);

  lv_obj_add_flag(ssid_row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ssid_row, wifi_open_picker_event_cb, LV_EVENT_CLICKED,
                      _UI);
}

static void ui_build_screen_settings(UI *_UI) {
  if (!_UI)
    return;

  _UI->screen_settings = lv_obj_create(_UI->content);
  _UI->active_screen = _UI->screen_settings;

  lv_obj_set_size(_UI->screen_settings, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(_UI->screen_settings, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(_UI->screen_settings, 0, 0);
  lv_obj_set_style_shadow_width(_UI->screen_settings, 0, 0);
  lv_obj_set_style_outline_width(_UI->screen_settings, 0, 0);
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

static void ui_build_screen_facility(UI *_UI) {
  if (!_UI)
    return;

  _UI->screen_facility = lv_obj_create(_UI->content);
  _UI->active_screen = _UI->screen_facility;

  lv_obj_set_size(_UI->screen_facility, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(_UI->screen_facility, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(_UI->screen_facility, 0, 0);
  lv_obj_set_style_shadow_width(_UI->screen_facility, 0, 0);
  lv_obj_set_style_outline_width(_UI->screen_facility, 0, 0);
  lv_obj_set_style_pad_all(_UI->screen_facility, 0, 0);
  lv_obj_set_style_radius(_UI->screen_facility, 0, 0);
  lv_obj_clear_flag(_UI->screen_facility, LV_OBJ_FLAG_SCROLLABLE);

  /* Moving form, not LVGL scroll */
  _UI->facility_form = lv_obj_create(_UI->screen_facility);
  lv_obj_set_width(_UI->facility_form, LV_PCT(100));
  lv_obj_set_height(_UI->facility_form, LV_PCT(100));
  lv_obj_set_pos(_UI->facility_form, 0, 0);

  lv_obj_set_style_bg_opa(_UI->facility_form, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(_UI->facility_form, 0, 0);
  lv_obj_set_style_shadow_width(_UI->facility_form, 0, 0);
  lv_obj_set_style_outline_width(_UI->facility_form, 0, 0);
  lv_obj_set_style_pad_all(_UI->facility_form, 0, 0);
  lv_obj_set_style_pad_right(_UI->facility_form, 48, 0);
  lv_obj_set_style_radius(_UI->facility_form, 0, 0);
  lv_obj_clear_flag(_UI->facility_form, LV_OBJ_FLAG_SCROLLABLE);

  const lv_coord_t row_h = 46;
  const lv_coord_t gap = 10;
  const lv_coord_t field_w = LV_PCT(100);
  lv_coord_t y = 0;

  _UI->facility_name_ta =
      ui_create_form_field(_UI->facility_form, "Facility name", y, row_h);
  y += row_h + gap;

  _UI->facility_country_ta =
      ui_create_form_field(_UI->facility_form, "Country", y, row_h);
  y += row_h + gap;

  _UI->facility_address_ta =
      ui_create_form_field(_UI->facility_form, "Street address", y, row_h);
  y += row_h + gap;

  _UI->facility_city_ta =
      ui_create_form_field(_UI->facility_form, "City", y, row_h);
  y += row_h + gap;

  _UI->facility_zip_ta =
      ui_create_form_field(_UI->facility_form, "ZIP / Postal code", y, row_h);
  y += row_h + gap;

  _UI->facility_state_ta =
      ui_create_form_field(_UI->facility_form, "State / Region", y, row_h);
  y += row_h + gap;

  _UI->facility_contact_ta =
      ui_create_form_field(_UI->facility_form, "Contact person", y, row_h);
  y += row_h + gap;

  _UI->facility_phone_ta =
      ui_create_form_field(_UI->facility_form, "Phone number", y, row_h);
  y += row_h + gap;

  _UI->facility_email_ta =
      ui_create_form_field(_UI->facility_form, "Email", y, row_h);
  y += row_h + gap;

  _UI->facility_notes_ta =
      ui_create_form_field(_UI->facility_form, "Notes", y, 80);
  lv_textarea_set_one_line(_UI->facility_notes_ta, false);
  y += 80 + gap;

  _UI->facility_save_btn = lv_btn_create(_UI->facility_form);
  lv_obj_set_size(_UI->facility_save_btn, field_w, 46);
  lv_obj_set_pos(_UI->facility_save_btn, 0, y);
  lv_obj_set_style_radius(_UI->facility_save_btn, 6, 0);
  lv_obj_set_style_shadow_width(_UI->facility_save_btn, 0, 0);
  lv_obj_set_style_outline_width(_UI->facility_save_btn, 0, 0);
  lv_obj_add_event_cb(_UI->facility_save_btn, fake_save_facility_event_cb,
                      LV_EVENT_CLICKED, _UI);

  lv_obj_t *btn_label = lv_label_create(_UI->facility_save_btn);
  lv_label_set_text(btn_label, "Save");
  lv_obj_center(btn_label);
  y += 46 + gap;

  _UI->facility_status_label = lv_label_create(_UI->facility_form);
  lv_obj_set_width(_UI->facility_status_label, field_w);
  lv_obj_set_pos(_UI->facility_status_label, 0, y);
  lv_label_set_text(_UI->facility_status_label, "");
  lv_label_set_long_mode(_UI->facility_status_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(_UI->facility_status_label, lv_color_white(), 0);
  y += 30 + gap;

  /* Actual form height. Buttons will move this object up/down. */
  lv_obj_set_height(_UI->facility_form, y + 20);

  lv_obj_t *scroll_up_btn = lv_btn_create(_UI->screen_facility);
  lv_obj_set_size(scroll_up_btn, 40, 40);
  lv_obj_align(scroll_up_btn, LV_ALIGN_RIGHT_MID, 0, -50);
  lv_obj_set_style_radius(scroll_up_btn, 8, 0);
  lv_obj_set_style_shadow_width(scroll_up_btn, 0, 0);
  lv_obj_set_style_outline_width(scroll_up_btn, 0, 0);
  lv_obj_add_event_cb(scroll_up_btn, facility_scroll_up_event_cb,
                      LV_EVENT_CLICKED, _UI);

  lv_obj_t *up_label = lv_label_create(scroll_up_btn);
  lv_label_set_text(up_label, LV_SYMBOL_UP);
  lv_obj_center(up_label);

  lv_obj_t *scroll_down_btn = lv_btn_create(_UI->screen_facility);
  lv_obj_set_size(scroll_down_btn, 40, 40);
  lv_obj_align(scroll_down_btn, LV_ALIGN_RIGHT_MID, 0, 10);
  lv_obj_set_style_radius(scroll_down_btn, 8, 0);
  lv_obj_set_style_shadow_width(scroll_down_btn, 0, 0);
  lv_obj_set_style_outline_width(scroll_down_btn, 0, 0);
  lv_obj_add_event_cb(scroll_down_btn, facility_scroll_down_event_cb,
                      LV_EVENT_CLICKED, _UI);

  lv_obj_t *down_label = lv_label_create(scroll_down_btn);
  lv_label_set_text(down_label, LV_SYMBOL_DOWN);
  lv_obj_center(down_label);

  lv_obj_move_foreground(scroll_up_btn);
  lv_obj_move_foreground(scroll_down_btn);
}

void ui_show_screen(UI *_UI, UI_Screen _screen) {
  if (!_UI)
    return;

  _UI->current_screen = _screen;

  ui_destroy_active_screen(_UI);
  ui_build_active_screen(_UI, _screen);

  switch (_screen) {
  case UI_SCREEN_HOME:
    lv_label_set_text(_UI->header_title, "ESPMaestro");
    lv_label_set_text(_UI->footer_label, "Overview");
    lv_obj_add_flag(_UI->back_btn, LV_OBJ_FLAG_HIDDEN);
    break;

  case UI_SCREEN_SETTINGS:
    lv_label_set_text(_UI->header_title, "Settings");
    lv_label_set_text(_UI->footer_label, "Choose a section");
    lv_obj_clear_flag(_UI->back_btn, LV_OBJ_FLAG_HIDDEN);
    break;

  case UI_SCREEN_WIFI:
    lv_label_set_text(_UI->header_title, "WiFi Configuration");
    lv_label_set_text(_UI->footer_label, "Enter credentials");
    lv_obj_clear_flag(_UI->back_btn, LV_OBJ_FLAG_HIDDEN);
    break;

  case UI_SCREEN_FACILITY:
    lv_label_set_text(_UI->header_title, "Facility Configuration");
    lv_label_set_text(_UI->footer_label, "Edit facility settings");
    lv_obj_clear_flag(_UI->back_btn, LV_OBJ_FLAG_HIDDEN);
    break;
  }
}

static void connect_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  if (!_UI)
    return;

  const char *ssid = NULL;

  if (_UI->wifi_network_count > 0) {
    ssid = _UI->wifi_networks[_UI->wifi_network_selected];
  }

  const char *pass = lv_textarea_get_text(_UI->wifi_pass_ta);

  if (!ssid || strlen(ssid) == 0) {
    ui_set_wifi_form_status(_UI, "Please scan and select a network", true);
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

  lv_label_set_text(_UI->facility_status_label, "Saved facility");
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
    return;
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
  if (!_UI || !_options)
    return;

  memset(_UI->wifi_networks, 0, sizeof(_UI->wifi_networks));
  _UI->wifi_network_count = 0;
  _UI->wifi_network_selected = 0;

  const char *p = _options;

  while (*p && _UI->wifi_network_count < 20) {
    char *dst = _UI->wifi_networks[_UI->wifi_network_count];
    size_t len = 0;

    while (*p && *p != '\n') {
      if (len < 63)
        dst[len++] = *p;
      p++;
    }

    if (*p == '\n')
      p++;

    dst[len] = '\0';

    if (len == 0)
      continue;

    if (strcmp(dst, "Scan for networks...") == 0)
      continue;

    if (strcmp(dst, "No networks found") == 0)
      continue;

    _UI->wifi_network_count++;
  }

  wifi_update_selected_network(_UI);

  ui_set_wifi_form_status(_UI,
                          _UI->wifi_network_count > 0 ? "Network selected"
                                                      : "No networks found",
                          _UI->wifi_network_count == 0);
}

static void wifi_ta_event_cb(lv_event_t *_event) {
  UI *_UI = (UI *)lv_event_get_user_data(_event);
  lv_obj_t *ta = lv_event_get_target(_event);
  lv_event_code_t code = lv_event_get_code(_event);

  if (!_UI || !ta || !_UI->wifi_keyboard)
    return;

  if (code == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(_UI->wifi_keyboard, ta);
    lv_obj_clear_flag(_UI->wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_height(_UI->wifi_keyboard, 160);
    lv_obj_update_layout(_UI->wifi_keyboard);

    if (_UI->wifi_form) {
      lv_obj_set_flex_grow(_UI->wifi_form, 0);
      lv_obj_set_height(_UI->wifi_form, 260);
    }
  } else if (code == LV_EVENT_DEFOCUSED) {
    lv_keyboard_set_textarea(_UI->wifi_keyboard, NULL);
    lv_obj_add_flag(_UI->wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    if (_UI->wifi_form) {
      lv_obj_set_flex_grow(_UI->wifi_form, 1);
    }
  }
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

static void facility_scroll_by(UI *_UI, int32_t delta) {
  if (!_UI || !_UI->facility_form || !_UI->screen_facility)
    return;

  int32_t cur_y = lv_obj_get_y(_UI->facility_form);
  int32_t form_h = lv_obj_get_height(_UI->facility_form);
  int32_t view_h = lv_obj_get_height(_UI->screen_facility);

  int32_t min_y = view_h - form_h;
  if (min_y > 0)
    min_y = 0;

  int32_t next_y = cur_y - delta;

  if (next_y > 0)
    next_y = 0;

  if (next_y < min_y)
    next_y = min_y;

  lv_obj_set_y(_UI->facility_form, next_y);
}

static void facility_scroll_up_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  facility_scroll_by(_UI, -150);
}

static void facility_scroll_down_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  facility_scroll_by(_UI, 150);
}

static void wifi_update_selected_network(UI *_UI) {
  if (!_UI || !_UI->wifi_ssid_label)
    return;

  if (_UI->wifi_network_count <= 0) {
    lv_label_set_text(_UI->wifi_ssid_label, "No networks found");
    return;
  }

  if (_UI->wifi_network_selected < 0)
    _UI->wifi_network_selected = 0;

  if (_UI->wifi_network_selected >= _UI->wifi_network_count)
    _UI->wifi_network_selected = _UI->wifi_network_count - 1;

  lv_label_set_text(_UI->wifi_ssid_label,
                    _UI->wifi_networks[_UI->wifi_network_selected]);
}

static void wifi_prev_network_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  if (!_UI || _UI->wifi_network_count <= 0)
    return;

  _UI->wifi_network_selected--;

  if (_UI->wifi_network_selected < 0) {
    _UI->wifi_network_selected = _UI->wifi_network_count - 1;
  }

  wifi_update_selected_network(_UI);
}

static void wifi_next_network_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  if (!_UI || _UI->wifi_network_count <= 0)
    return;

  _UI->wifi_network_selected++;

  if (_UI->wifi_network_selected >= _UI->wifi_network_count)
    _UI->wifi_network_selected = 0;

  wifi_update_selected_network(_UI);
}

static void wifi_picker_refresh(UI *_UI) {
  if (!_UI || !_UI->wifi_picker_overlay)
    return;

  for (int i = 0; i < 10; i++) {
    int idx = _UI->wifi_picker_top + i;

    if (idx < _UI->wifi_network_count) {
      lv_obj_clear_flag(_UI->wifi_picker_rows[i], LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text(_UI->wifi_picker_row_labels[i],
                        _UI->wifi_networks[idx]);

      if (idx == _UI->wifi_picker_cursor) {
        lv_obj_set_style_bg_color(_UI->wifi_picker_rows[i],
                                  lv_color_hex(0x1E88E5), 0);
      } else {
        lv_obj_set_style_bg_color(_UI->wifi_picker_rows[i],
                                  lv_color_hex(0x050709), 0);
      }
    } else {
      lv_obj_add_flag(_UI->wifi_picker_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void wifi_picker_close(UI *_UI) {
  if (!_UI || !_UI->wifi_picker_overlay)
    return;

  lv_obj_del(_UI->wifi_picker_overlay);
  _UI->wifi_picker_overlay = NULL;
  _UI->wifi_picker_panel = NULL;

  for (int i = 0; i < 10; i++) {
    _UI->wifi_picker_rows[i] = NULL;
    _UI->wifi_picker_row_labels[i] = NULL;
  }
}

static void wifi_open_picker_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  if (!_UI || !_UI->screen_wifi)
    return;

  if (_UI->wifi_network_count <= 0) {
    ui_set_wifi_form_status(_UI, "Scan networks first", true);
    return;
  }

  _UI->wifi_picker_cursor = _UI->wifi_network_selected;
  _UI->wifi_picker_top = _UI->wifi_picker_cursor - 4;
  if (_UI->wifi_picker_top < 0)
    _UI->wifi_picker_top = 0;

  if (_UI->wifi_picker_top > _UI->wifi_network_count - 10)
    _UI->wifi_picker_top = _UI->wifi_network_count - 10;

  if (_UI->wifi_picker_top < 0)
    _UI->wifi_picker_top = 0;

  _UI->wifi_picker_overlay = lv_obj_create(_UI->screen_wifi);
  lv_obj_set_size(_UI->wifi_picker_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(_UI->wifi_picker_overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(_UI->wifi_picker_overlay, LV_OPA_70, 0);
  lv_obj_set_style_border_width(_UI->wifi_picker_overlay, 0, 0);
  lv_obj_set_style_pad_all(_UI->wifi_picker_overlay, 8, 0);
  lv_obj_clear_flag(_UI->wifi_picker_overlay, LV_OBJ_FLAG_SCROLLABLE);

  _UI->wifi_picker_panel = lv_obj_create(_UI->wifi_picker_overlay);
  lv_obj_set_size(_UI->wifi_picker_panel, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(_UI->wifi_picker_panel, lv_color_hex(0x111315), 0);
  lv_obj_set_style_border_width(_UI->wifi_picker_panel, 1, 0);
  lv_obj_set_style_border_color(_UI->wifi_picker_panel, lv_color_hex(0x333333),
                                0);
  lv_obj_set_style_radius(_UI->wifi_picker_panel, 10, 0);
  lv_obj_set_style_pad_all(_UI->wifi_picker_panel, 8, 0);
  lv_obj_clear_flag(_UI->wifi_picker_panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(_UI->wifi_picker_panel);
  lv_label_set_text(title, "Select Network");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 2, 0);

  const lv_coord_t row_h = 24;
  const lv_coord_t list_y = 26;
  const lv_coord_t btn_w = 38;

  for (int i = 0; i < 10; i++) {
    _UI->wifi_picker_rows[i] = lv_obj_create(_UI->wifi_picker_panel);
    lv_obj_set_size(_UI->wifi_picker_rows[i], LV_PCT(100), row_h);
    lv_obj_set_pos(_UI->wifi_picker_rows[i], 0, list_y + i * row_h);
    lv_obj_set_style_bg_color(_UI->wifi_picker_rows[i], lv_color_hex(0x050709),
                              0);
    lv_obj_set_style_border_width(_UI->wifi_picker_rows[i], 1, 0);
    lv_obj_set_style_border_color(_UI->wifi_picker_rows[i],
                                  lv_color_hex(0x222222), 0);
    lv_obj_set_style_radius(_UI->wifi_picker_rows[i], 4, 0);
    lv_obj_set_style_pad_left(_UI->wifi_picker_rows[i], 6, 0);
    lv_obj_clear_flag(_UI->wifi_picker_rows[i], LV_OBJ_FLAG_SCROLLABLE);

    _UI->wifi_picker_row_labels[i] = lv_label_create(_UI->wifi_picker_rows[i]);
    lv_label_set_long_mode(_UI->wifi_picker_row_labels[i], LV_LABEL_LONG_DOT);
    lv_obj_set_width(_UI->wifi_picker_row_labels[i], LV_PCT(82));
    lv_obj_set_style_text_color(_UI->wifi_picker_row_labels[i],
                                lv_color_white(), 0);
    lv_obj_align(_UI->wifi_picker_row_labels[i], LV_ALIGN_LEFT_MID, 4, 0);
  }

  _UI->wifi_picker_up_btn = lv_btn_create(_UI->wifi_picker_panel);
  lv_obj_set_size(_UI->wifi_picker_up_btn, btn_w, 38);
  lv_obj_align(_UI->wifi_picker_up_btn, LV_ALIGN_RIGHT_MID, 0, -36);
  lv_obj_add_event_cb(_UI->wifi_picker_up_btn, wifi_picker_up_event_cb,
                      LV_EVENT_CLICKED, _UI);

  lv_obj_t *up = lv_label_create(_UI->wifi_picker_up_btn);
  lv_label_set_text(up, LV_SYMBOL_UP);
  lv_obj_center(up);

  _UI->wifi_picker_down_btn = lv_btn_create(_UI->wifi_picker_panel);
  lv_obj_set_size(_UI->wifi_picker_down_btn, btn_w, 38);
  lv_obj_align(_UI->wifi_picker_down_btn, LV_ALIGN_RIGHT_MID, 0, 10);
  lv_obj_add_event_cb(_UI->wifi_picker_down_btn, wifi_picker_down_event_cb,
                      LV_EVENT_CLICKED, _UI);

  lv_obj_t *down = lv_label_create(_UI->wifi_picker_down_btn);
  lv_label_set_text(down, LV_SYMBOL_DOWN);
  lv_obj_center(down);

  _UI->wifi_picker_cancel_btn = lv_btn_create(_UI->wifi_picker_panel);
  lv_obj_set_size(_UI->wifi_picker_cancel_btn, 70, 34);
  lv_obj_align(_UI->wifi_picker_cancel_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_event_cb(_UI->wifi_picker_cancel_btn, wifi_picker_cancel_event_cb,
                      LV_EVENT_CLICKED, _UI);

  lv_obj_t *cancel = lv_label_create(_UI->wifi_picker_cancel_btn);
  lv_label_set_text(cancel, "Cancel");
  lv_obj_center(cancel);

  _UI->wifi_picker_ok_btn = lv_btn_create(_UI->wifi_picker_panel);
  lv_obj_set_size(_UI->wifi_picker_ok_btn, 70, 34);
  lv_obj_align(_UI->wifi_picker_ok_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(_UI->wifi_picker_ok_btn, wifi_picker_ok_event_cb,
                      LV_EVENT_CLICKED, _UI);

  lv_obj_t *ok = lv_label_create(_UI->wifi_picker_ok_btn);
  lv_label_set_text(ok, "OK");
  lv_obj_center(ok);

  wifi_picker_refresh(_UI);
}

static void wifi_picker_up_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  if (!_UI || _UI->wifi_network_count <= 0)
    return;

  if (_UI->wifi_picker_cursor > 0)
    _UI->wifi_picker_cursor--;

  if (_UI->wifi_picker_cursor < _UI->wifi_picker_top)
    _UI->wifi_picker_top = _UI->wifi_picker_cursor;

  wifi_picker_refresh(_UI);
}

static void wifi_picker_down_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  if (!_UI || _UI->wifi_network_count <= 0)
    return;

  if (_UI->wifi_picker_cursor < _UI->wifi_network_count - 1)
    _UI->wifi_picker_cursor++;

  if (_UI->wifi_picker_cursor >= _UI->wifi_picker_top + 10)
    _UI->wifi_picker_top = _UI->wifi_picker_cursor - 9;

  wifi_picker_refresh(_UI);
}

static void wifi_picker_ok_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  if (!_UI)
    return;

  _UI->wifi_network_selected = _UI->wifi_picker_cursor;
  wifi_update_selected_network(_UI);
  wifi_picker_close(_UI);
}

static void wifi_picker_cancel_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  wifi_picker_close(_UI);
}

void ui_tick(UI *_UI) { (void)_UI; }
