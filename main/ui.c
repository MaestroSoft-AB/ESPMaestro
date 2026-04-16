#include "ui.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

extern const lv_font_t notosans_14;
static const char *TAG = "UI";

/*--------------------Internal Helpers-----------------*/
static void ui_build_root(UI *_UI);
static void ui_build_header(UI *_UI);
static void ui_build_body(UI *_UI);
static void ui_build_footer(UI *_UI);
static lv_obj_t *ui_create_button(UI *_UI, lv_obj_t *_parent,
                                  const char *_text);
static void header_button_event_cb(lv_event_t *_event);
/*----------------------------------------------------*/

void ui_init(UI *_UI) {
  if (!_UI)
    return;

  memset(_UI, 0, sizeof(UI));
  snprintf(_UI->wifi_status, sizeof(_UI->wifi_status), "WIFI: Not connected");

  ui_build_root(_UI);
  ui_build_header(_UI);
  ui_build_body(_UI);
  ui_build_footer(_UI);
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
  lv_obj_set_height(_UI->header, 60);
  lv_obj_set_width(_UI->header, LV_PCT(100));

  lv_obj_set_layout(_UI->header, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(_UI->header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  // SPACE_BETWEEN pushes title and button row to opposite sides

  lv_obj_set_style_radius(_UI->header, 0, 0);
  lv_obj_set_style_border_width(_UI->header, 0, 0);
  lv_obj_set_style_pad_left(_UI->header, 12, 0);
  lv_obj_set_style_pad_right(_UI->header, 12, 0);
  lv_obj_set_style_pad_top(_UI->header, 8, 0);
  lv_obj_set_style_pad_bottom(_UI->header, 8, 0);
  lv_obj_set_style_bg_color(_UI->header, lv_color_hex(0x101820), 0);

  /*--------------TITLE-------------------*/
  _UI->header_title = lv_label_create(_UI->header);
  lv_obj_set_style_text_color(_UI->header_title, lv_color_white(), 0);
  lv_obj_set_style_text_font(_UI->header_title, &notosans_14, 0);
  lv_label_set_text(_UI->header_title, "ESPMaestro");

  /*--------------BUTTONS-------------------*/
  _UI->header_btn_row = lv_obj_create(_UI->header);
  lv_obj_set_layout(_UI->header_btn_row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->header_btn_row, LV_FLEX_FLOW_ROW);

  lv_obj_set_style_pad_all(_UI->header_btn_row, 0, 0);
  lv_obj_set_style_pad_gap(_UI->header_btn_row, 8, 0);
  // pad_gap controls spacing between flex children

  lv_obj_set_style_bg_opa(_UI->header_btn_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(_UI->header_btn_row, 0, 0);

  lv_obj_set_width(_UI->header_btn_row, LV_SIZE_CONTENT);
  lv_obj_set_height(_UI->header_btn_row, LV_SIZE_CONTENT);
  // LV_SIZE_CONTENT lets the container shrink-wrap around its children

  _UI->btn_menu = ui_create_button(_UI, _UI->header_btn_row, "Menu");
  _UI->btn_wifi = ui_create_button(_UI, _UI->header_btn_row, "WiFi");
}
void ui_build_body(UI *_UI) {
  if (!_UI)
    return;

  _UI->body = lv_obj_create(_UI->root);
  lv_obj_set_width(_UI->body, LV_PCT(100));
  lv_obj_set_flex_grow(_UI->body, 1);
  // flex_grow = 1 makes body consume the remaining space between header and
  // footer

  lv_obj_set_style_radius(_UI->body, 0, 0);
  lv_obj_set_style_border_width(_UI->body, 0, 0);
  lv_obj_set_style_pad_all(_UI->body, 16, 0);
  lv_obj_set_style_bg_color(_UI->body, lv_color_black(), 0);

  _UI->body_label = lv_label_create(_UI->body);

  lv_obj_set_width(_UI->body_label, LV_PCT(100));
  // The explicit width gives wrapping a known line width

  lv_obj_set_style_text_color(_UI->body_label, lv_color_hex(0x32CD32), 0);
  lv_obj_set_style_text_font(_UI->body_label, &notosans_14, 0);
  lv_label_set_long_mode(_UI->body_label, LV_LABEL_LONG_WRAP);
  // Wrap long text instead of letting it extend beyond the label width
}
static void ui_build_footer(UI *_UI) {
  if (!_UI)
    return;

  _UI->footer = lv_obj_create(_UI->root);
  lv_obj_set_width(_UI->footer, LV_PCT(100));
  lv_obj_set_height(_UI->footer, 40);

  lv_obj_set_style_radius(_UI->footer, 0, 0);
  lv_obj_set_style_border_width(_UI->footer, 0, 0);
  lv_obj_set_style_pad_left(_UI->footer, 12, 0);
  lv_obj_set_style_pad_right(_UI->footer, 12, 0);
  lv_obj_set_style_pad_top(_UI->footer, 8, 0);
  lv_obj_set_style_pad_bottom(_UI->footer, 8, 0);
  lv_obj_set_style_bg_color(_UI->footer, lv_color_hex(0x101820), 0);

  _UI->footer_label = lv_label_create(_UI->footer);
  lv_obj_set_style_text_color(_UI->footer_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(_UI->footer_label, &notosans_14, 0);
  lv_label_set_text(_UI->footer_label, "Fakta");
}
lv_obj_t *ui_create_button(UI *_UI, lv_obj_t *_parent, const char *_text) {
  if (!_UI || !_parent)
    return NULL;

  lv_obj_t *btn = lv_btn_create(_parent);
  lv_obj_set_size(btn, 90, 40);

  lv_obj_add_event_cb(btn, header_button_event_cb, LV_EVENT_CLICKED, _UI);

  lv_obj_t *label = lv_label_create(btn);
  lv_obj_set_style_text_font(label, &notosans_14, 0);
  lv_label_set_text(label, _text);
  lv_obj_center(label);

  return btn;
}

static void header_button_event_cb(lv_event_t *_event) {
  if (!_event)
    return;

  if (lv_event_get_code(_event) != LV_EVENT_CLICKED)
    return;

  UI *_UI = (UI *)lv_event_get_user_data(_event);
  lv_obj_t *btn = lv_event_get_target(_event);

  if (!_UI || !btn)
    return;

  if (btn == _UI->btn_menu) {
    ESP_LOGI(TAG, "Menu button was pressed");
    lv_label_set_text(_UI->footer_label, "Menu button was pressed");
    lv_label_set_text(_UI->body_label, "VERY COOL MENU SHOWS UP HERE");
  } else if (btn == _UI->btn_wifi) {
    ESP_LOGI(TAG, "WiFi button was pressed");
    lv_label_set_text(_UI->footer_label, "WiFi button was pressed");
    lv_label_set_text(_UI->body_label, "SHOW WIFI MENU ON SIDE?");
  }
}

void ui_set_body_text(UI *_UI, const char *_text) {
  if (!_UI || !_UI->body_label || !_text)
    return;

  lv_label_set_text(_UI->body_label, _text);
}

void ui_set_footer_text(UI *_UI, const char *_text) {
  if (!_UI || !_UI->footer_label || !_text)
    return;

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

void ui_tick(UI *_UI) { (void)_UI; }
