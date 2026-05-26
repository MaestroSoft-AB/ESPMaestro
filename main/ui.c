#include "ui.h"
#include "esp_log.h"
#include "wifi_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const lv_font_t notosans_14;
static const char *TAG = "UI";

#define C_BLACK 0x000000
#define C_PANEL 0x111827
#define C_CARD 0x1F2937
#define C_CARD_DARK 0x111827
#define C_BORDER 0x374151
#define C_TEXT 0xFFFFFF
#define C_MUTED 0x9CA3AF
#define C_BLUE 0x2563EB
#define C_GREEN 0x22C55E
#define C_YELLOW 0xEAB308
#define C_PURPLE 0x9333EA
#define C_RED 0xEF4444

static void ui_build_root(UI *_UI);
static void ui_build_nav(UI *_UI);
static void ui_build_content(UI *_UI);
static void ui_build_footer(UI *_UI);
static void ui_destroy_active_screen(UI *_UI);
static void ui_build_active_screen(UI *_UI, UI_Screen _screen);
static void ui_update_nav(UI *_UI);

static void ui_build_screen_home(UI *_UI);
static void ui_build_screen_forecast(UI *_UI);
static void ui_build_screen_elpriser(UI *_UI);
static void ui_build_screen_settings(UI *_UI);
static void ui_build_screen_wifi(UI *_UI);
static void ui_build_screen_facility(UI *_UI);
static void ui_build_screen_device_info(UI *_UI);
static void keyboard_event_cb(lv_event_t *e);
static void ui_hide_keyboard(UI *_UI);
static lv_obj_t *ui_create_panel(lv_obj_t *_parent);
static lv_obj_t *ui_create_label(lv_obj_t *_parent, const char *_text,
                                 lv_color_t _color);
static lv_obj_t *ui_create_button(lv_obj_t *_parent, const char *_text,
                                  lv_color_t _bg);
static lv_obj_t *ui_create_nav_button(lv_obj_t *_parent, const char *_text,
                                      UI_Screen _screen, UI *_UI);
static lv_obj_t *ui_create_textarea(UI *_UI, lv_obj_t *parent,
                                    const char *placeholder);
static lv_obj_t *ui_create_form_field(UI *_UI, lv_obj_t *_parent,
                                      const char *_label,
                                      const char *_placeholder);
static void ui_create_metric_card(lv_obj_t *_parent, const char *_title,
                                  const char *_value, const char *_sub,
                                  lv_color_t _accent);
static void ui_create_settings_card(UI *_UI, lv_obj_t *_parent, lv_obj_t **_out,
                                    const char *_title, const char *_sub,
                                    lv_color_t _accent);
static void ui_fill_chart(lv_obj_t *_chart, const int *_values, int _count,
                          lv_color_t _color);
static void ui_update_wifi_rows(UI *_UI);
static void ui_open_wifi_password(UI *_UI, int _idx);
static void ui_close_wifi_password(UI *_UI);
static void ui_build_facility_page(UI *_UI);

static void nav_event_cb(lv_event_t *_event);
static void back_event_cb(lv_event_t *_event);
static void settings_card_event_cb(lv_event_t *_event);
static void view_toggle_event_cb(lv_event_t *_event);
static void wifi_scan_btn_event_cb(lv_event_t *_event);
static void wifi_network_row_event_cb(lv_event_t *_event);
static void wifi_prev_page_event_cb(lv_event_t *_event);
static void wifi_next_page_event_cb(lv_event_t *_event);
static void connect_event_cb(lv_event_t *_event);
static void wifi_cancel_event_cb(lv_event_t *_event);
static void facility_prev_event_cb(lv_event_t *_event);
static void facility_next_event_cb(lv_event_t *_event);
static void facility_save_event_cb(lv_event_t *_event);

/****************** HELP ME *************************/

static void facility_read_current_page(UI *_UI) {
  if (!_UI)
    return;

  if (_UI->facility_page == 0) {
    if (_UI->facility_name_ta) {
      snprintf(_UI->facility_cfg.facility_name,
               sizeof(_UI->facility_cfg.facility_name), "%s",
               lv_textarea_get_text(_UI->facility_name_ta));
    }

    if (_UI->facility_address_ta) {
      snprintf(_UI->facility_cfg.address, sizeof(_UI->facility_cfg.address),
               "%s", lv_textarea_get_text(_UI->facility_address_ta));
    }

  } else if (_UI->facility_page == 1) {
    if (_UI->facility_city_ta) {
      snprintf(_UI->facility_cfg.city, sizeof(_UI->facility_cfg.city), "%s",
               lv_textarea_get_text(_UI->facility_city_ta));
    }

    if (_UI->facility_zip_ta) {
      _UI->facility_cfg.zip =
          (uint8_t)atoi(lv_textarea_get_text(_UI->facility_zip_ta));
    }

  } else {
    if (_UI->facility_lat_ta) {
      snprintf(_UI->facility_cfg.lat, sizeof(_UI->facility_cfg.lat), "%s",
               lv_textarea_get_text(_UI->facility_lat_ta));
    }

    if (_UI->facility_lon_ta) {
      snprintf(_UI->facility_cfg.lon, sizeof(_UI->facility_cfg.lon), "%s",
               lv_textarea_get_text(_UI->facility_lon_ta));
    }

    if (_UI->facility_energy_zone_ta) {
      _UI->facility_cfg.energy_zone =
          (uint8_t)atoi(lv_textarea_get_text(_UI->facility_energy_zone_ta));
    }
  }
}
static void textarea_event_cb(lv_event_t *e) {
  UI *ui = lv_event_get_user_data(e);
  lv_obj_t *ta = lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);

  if (!ui || !ui->keyboard)
    return;

  if (code == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(ui->keyboard, ta);
    lv_obj_clear_flag(ui->keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui->keyboard);
  }
}

/*****************************************************/
void ui_init(UI *_UI) {
  if (!_UI)
    return;
  memset(_UI, 0, sizeof(UI));
  _UI->wifi_connected = false;
  _UI->wifi_connecting_index = -1;
  _UI->forecast_view_mode = UI_VIEW_GRAPH;
  _UI->elpriser_view_mode = UI_VIEW_GRAPH;
  snprintf(_UI->wifi_status, sizeof(_UI->wifi_status), "WiFi: Not connected");
  ESP_LOGI(TAG, "Initializing Figma UI...");
  ui_build_root(_UI);
  ui_build_nav(_UI);
  ui_build_content(_UI);
  ui_build_footer(_UI);
  _UI->keyboard = lv_keyboard_create(lv_scr_act());
  lv_obj_set_size(_UI->keyboard, LV_PCT(100), 220);
  lv_obj_align(_UI->keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(_UI->keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(_UI->keyboard, keyboard_event_cb, LV_EVENT_READY, _UI);
  lv_obj_add_event_cb(_UI->keyboard, keyboard_event_cb, LV_EVENT_CANCEL, _UI);
  ui_show_screen(_UI, UI_SCREEN_HOME);
}

static void ui_build_root(UI *_UI) {
  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(C_BLACK), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  _UI->root = lv_obj_create(screen);
  lv_obj_set_size(_UI->root, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(_UI->root, lv_color_hex(C_BLACK), 0);
  lv_obj_set_style_border_width(_UI->root, 0, 0);
  lv_obj_set_style_radius(_UI->root, 0, 0);
  lv_obj_set_style_pad_all(_UI->root, 0, 0);
  lv_obj_set_style_shadow_width(_UI->root, 0, 0);
  lv_obj_set_layout(_UI->root, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->root, LV_FLEX_FLOW_COLUMN);
}

static void ui_build_nav(UI *_UI) {
  _UI->nav = lv_obj_create(_UI->root);
  lv_obj_set_size(_UI->nav, LV_PCT(100), 86);
  lv_obj_set_style_bg_color(_UI->nav, lv_color_hex(C_PANEL), 0);
  lv_obj_set_style_border_width(_UI->nav, 1, 0);
  lv_obj_set_style_border_side(_UI->nav, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_color(_UI->nav, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_radius(_UI->nav, 0, 0);
  lv_obj_set_style_pad_all(_UI->nav, 16, 0);
  lv_obj_set_style_pad_gap(_UI->nav, 12, 0);
  lv_obj_set_layout(_UI->nav, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->nav, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(_UI->nav, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(_UI->nav, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(_UI->nav, LV_OBJ_FLAG_SCROLL_CHAIN);
  lv_obj_set_scrollbar_mode(_UI->nav, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *left_group = lv_obj_create(_UI->nav);
  lv_obj_set_size(left_group, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(left_group, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(left_group, 0, 0);
  lv_obj_set_style_pad_all(left_group, 0, 0);
  lv_obj_set_style_pad_gap(left_group, 12, 0);
  lv_obj_set_layout(left_group, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(left_group, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(left_group, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(left_group, LV_OBJ_FLAG_SCROLL_CHAIN);
  lv_obj_set_scrollbar_mode(left_group, LV_SCROLLBAR_MODE_OFF);

  _UI->nav_home_btn =
      ui_create_nav_button(left_group, LV_SYMBOL_HOME, UI_SCREEN_HOME, _UI);
  _UI->nav_forecast_btn = ui_create_nav_button(left_group, LV_SYMBOL_UPLOAD,
                                               UI_SCREEN_FORECAST, _UI);
  _UI->nav_elpriser_btn = ui_create_nav_button(left_group, LV_SYMBOL_CHARGE,
                                               UI_SCREEN_ELPRISER, _UI);

  lv_obj_t *left_spacer = lv_obj_create(_UI->nav);
  lv_obj_set_flex_grow(left_spacer, 1);
  lv_obj_set_height(left_spacer, 1);
  lv_obj_set_style_bg_opa(left_spacer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(left_spacer, 0, 0);
  lv_obj_clear_flag(left_spacer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(left_spacer, LV_OBJ_FLAG_SCROLL_CHAIN);
  lv_obj_set_scrollbar_mode(left_spacer, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *time_box = lv_obj_create(_UI->nav);
  lv_obj_set_size(time_box, 180, 54);
  lv_obj_set_style_bg_opa(time_box, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(time_box, 0, 0);
  lv_obj_set_style_pad_all(time_box, 0, 0);
  lv_obj_set_style_pad_gap(time_box, 2, 0);
  lv_obj_set_layout(time_box, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(time_box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(time_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(time_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(time_box, LV_OBJ_FLAG_SCROLL_CHAIN);
  lv_obj_set_scrollbar_mode(time_box, LV_SCROLLBAR_MODE_OFF);

  _UI->nav_clock_label = lv_label_create(time_box);
  lv_obj_set_style_text_color(_UI->nav_clock_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(_UI->nav_clock_label, &notosans_14, 0);
  lv_label_set_text(_UI->nav_clock_label, "--:--:--");

  _UI->nav_date_label = lv_label_create(time_box);
  lv_obj_set_style_text_color(_UI->nav_date_label, lv_color_hex(C_MUTED), 0);
  lv_obj_set_style_text_font(_UI->nav_date_label, &notosans_14, 0);
  lv_label_set_text(_UI->nav_date_label, "----------");

  lv_obj_t *right_spacer = lv_obj_create(_UI->nav);
  lv_obj_set_flex_grow(right_spacer, 1);
  lv_obj_set_height(right_spacer, 1);
  lv_obj_set_style_bg_opa(right_spacer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(right_spacer, 0, 0);
  lv_obj_clear_flag(right_spacer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(right_spacer, LV_OBJ_FLAG_SCROLL_CHAIN);
  lv_obj_set_scrollbar_mode(right_spacer, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *right_group = lv_obj_create(_UI->nav);
  lv_obj_set_size(right_group, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(right_group, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(right_group, 0, 0);
  lv_obj_set_style_pad_all(right_group, 0, 0);
  lv_obj_set_style_pad_gap(right_group, 12, 0);
  lv_obj_set_layout(right_group, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(right_group, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(right_group, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(right_group, LV_OBJ_FLAG_SCROLL_CHAIN);
  lv_obj_set_scrollbar_mode(right_group, LV_SCROLLBAR_MODE_OFF);

  _UI->nav_settings_btn = ui_create_nav_button(right_group, LV_SYMBOL_SETTINGS,
                                               UI_SCREEN_SETTINGS, _UI);
  _UI->nav_wifi_btn =
      ui_create_nav_button(right_group, LV_SYMBOL_WIFI, UI_SCREEN_WIFI, _UI);

  _UI->wifi_indicator = lv_obj_create(_UI->nav_wifi_btn);
  lv_obj_set_size(_UI->wifi_indicator, 9, 9);
  lv_obj_align(_UI->wifi_indicator, LV_ALIGN_TOP_RIGHT, -5, 5);
  lv_obj_set_style_radius(_UI->wifi_indicator, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(_UI->wifi_indicator, lv_color_hex(C_GREEN), 0);
  lv_obj_set_style_border_width(_UI->wifi_indicator, 0, 0);
  lv_obj_add_flag(_UI->wifi_indicator, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(_UI->wifi_indicator, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(_UI->wifi_indicator, LV_OBJ_FLAG_SCROLL_CHAIN);
  lv_obj_set_scrollbar_mode(_UI->wifi_indicator, LV_SCROLLBAR_MODE_OFF);
}

static void ui_build_content(UI *_UI) {
  _UI->content = lv_obj_create(_UI->root);
  lv_obj_set_width(_UI->content, LV_PCT(100));
  lv_obj_set_flex_grow(_UI->content, 1);
  lv_obj_set_style_bg_color(_UI->content, lv_color_hex(C_BLACK), 0);
  lv_obj_set_style_border_width(_UI->content, 0, 0);
  lv_obj_set_style_radius(_UI->content, 0, 0);
  lv_obj_set_style_pad_all(_UI->content, 0, 0);
  lv_obj_set_style_shadow_width(_UI->content, 0, 0);
}

static void ui_build_footer(UI *_UI) {
  _UI->footer = lv_obj_create(_UI->root);
  lv_obj_set_size(_UI->footer, LV_PCT(100), 34);
  lv_obj_set_style_bg_color(_UI->footer, lv_color_hex(0x0B1220), 0);
  lv_obj_set_style_border_width(_UI->footer, 0, 0);
  lv_obj_set_style_radius(_UI->footer, 0, 0);
  lv_obj_set_style_pad_left(_UI->footer, 12, 0);
  lv_obj_clear_flag(_UI->footer, LV_OBJ_FLAG_SCROLLABLE);
  _UI->footer_label = lv_label_create(_UI->footer);
  lv_obj_set_style_text_color(_UI->footer_label, lv_color_hex(C_MUTED), 0);
  lv_obj_set_style_text_font(_UI->footer_label, &notosans_14, 0);
  lv_label_set_text(_UI->footer_label, _UI->wifi_status);
  lv_obj_align(_UI->footer_label, LV_ALIGN_LEFT_MID, 0, 0);
}

static lv_obj_t *ui_create_nav_button(lv_obj_t *_parent, const char *_text,
                                      UI_Screen _screen, UI *_UI) {
  lv_obj_t *btn = lv_btn_create(_parent);
  lv_obj_set_size(btn, 56, 56);
  lv_obj_set_style_radius(btn, 8, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(C_CARD), 0);
  lv_obj_add_event_cb(btn, nav_event_cb, LV_EVENT_CLICKED, _UI);
  lv_obj_set_user_data(btn, (void *)(uintptr_t)_screen);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLL_CHAIN);
  lv_obj_set_scrollbar_mode(btn, LV_SCROLLBAR_MODE_OFF);
  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, _text);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_center(label);
  return btn;
}

static void ui_destroy_active_screen(UI *_UI) {
  if (!_UI || !_UI->active_screen)
    return;
  lv_obj_del(_UI->active_screen);
  _UI->active_screen = NULL;
  _UI->screen_home = NULL;
  _UI->screen_forecast = NULL;
  _UI->screen_elpriser = NULL;
  _UI->screen_settings = NULL;
  _UI->screen_wifi = NULL;
  _UI->screen_facility = NULL;
  _UI->screen_device_info = NULL;
  _UI->back_btn = NULL;
  _UI->card_wifi = NULL;
  _UI->card_facility = NULL;
  _UI->card_device_info = NULL;
  _UI->forecast_chart = NULL;
  _UI->forecast_table = NULL;
  _UI->forecast_graph_btn = NULL;
  _UI->forecast_table_btn = NULL;
  _UI->elpriser_chart = NULL;
  _UI->elpriser_table = NULL;
  _UI->elpriser_graph_btn = NULL;
  _UI->elpriser_table_btn = NULL;
  _UI->wifi_pass_ta = NULL;
  _UI->wifi_status_label = NULL;
  _UI->wifi_connect_btn = NULL;
  _UI->wifi_scan_btn = NULL;
  _UI->wifi_ssid_label = NULL;
  _UI->wifi_prev_btn = NULL;
  _UI->wifi_next_btn = NULL;
  _UI->wifi_page_label = NULL;
  _UI->wifi_password_overlay = NULL;
  _UI->wifi_password_panel = NULL;
  for (int i = 0; i < 5; i++) {
    _UI->wifi_network_rows[i] = NULL;
    _UI->wifi_network_labels[i] = NULL;
  }
  _UI->facility_form = NULL;
  _UI->facility_name_ta = NULL;
  _UI->facility_address_ta = NULL;
  _UI->facility_city_ta = NULL;
  _UI->facility_zip_ta = NULL;
  _UI->facility_status_label = NULL;
  _UI->facility_save_btn = NULL;
  _UI->facility_lat_ta = NULL;
  _UI->facility_lon_ta = NULL;
  _UI->facility_energy_zone_ta = NULL;
}

static void ui_build_active_screen(UI *_UI, UI_Screen _screen) {
  switch (_screen) {
  case UI_SCREEN_HOME:
    ui_build_screen_home(_UI);
    break;
  case UI_SCREEN_FORECAST:
    ui_build_screen_forecast(_UI);
    break;
  case UI_SCREEN_ELPRISER:
    ui_build_screen_elpriser(_UI);
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
  case UI_SCREEN_DEVICE_INFO:
    ui_build_screen_device_info(_UI);
    break;
  default:
    ui_build_screen_home(_UI);
    break;
  }
}

void ui_show_screen(UI *_UI, UI_Screen _screen) {
  if (!_UI)
    return;

  if (_UI->keyboard) {
    lv_keyboard_set_textarea(_UI->keyboard, NULL);
    lv_obj_add_flag(_UI->keyboard, LV_OBJ_FLAG_HIDDEN);
  }

  _UI->current_screen = _screen;
  ui_destroy_active_screen(_UI);
  ui_build_active_screen(_UI, _screen);
  ui_update_nav(_UI);
}

static void ui_update_nav(UI *_UI) {
  lv_obj_t *buttons[] = {_UI->nav_home_btn, _UI->nav_forecast_btn,
                         _UI->nav_elpriser_btn, _UI->nav_settings_btn,
                         _UI->nav_wifi_btn};
  UI_Screen screens[] = {UI_SCREEN_HOME, UI_SCREEN_FORECAST, UI_SCREEN_ELPRISER,
                         UI_SCREEN_SETTINGS, UI_SCREEN_WIFI};
  for (int i = 0; i < 5; i++) {
    if (!buttons[i])
      continue;
    if (screens[i] == _UI->current_screen) {
      lv_obj_set_style_bg_color(buttons[i], lv_color_hex(C_BLUE), 0);
      lv_obj_set_style_border_color(buttons[i], lv_color_hex(C_BLUE), 0);
    } else {
      lv_obj_set_style_bg_color(buttons[i], lv_color_hex(C_CARD), 0);
      lv_obj_set_style_border_color(buttons[i], lv_color_hex(C_BORDER), 0);
    }
  }
  if (_UI->wifi_indicator) {
    if (_UI->wifi_connected)
      lv_obj_clear_flag(_UI->wifi_indicator, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(_UI->wifi_indicator, LV_OBJ_FLAG_HIDDEN);
  }
}

static lv_obj_t *ui_create_panel(lv_obj_t *_parent) {
  lv_obj_t *panel = lv_obj_create(_parent);
  lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(panel, lv_color_hex(C_PANEL), 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_radius(panel, 10, 0);
  lv_obj_set_style_pad_all(panel, 24, 0);
  lv_obj_set_style_shadow_width(panel, 0, 0);
  return panel;
}

static lv_obj_t *ui_create_label(lv_obj_t *_parent, const char *_text,
                                 lv_color_t _color) {
  lv_obj_t *label = lv_label_create(_parent);
  lv_label_set_text(label, _text);
  lv_obj_set_style_text_color(label, _color, 0);
  lv_obj_set_style_text_font(label, &notosans_14, 0);
  return label;
}

static lv_obj_t *ui_create_button(lv_obj_t *_parent, const char *_text,
                                  lv_color_t _bg) {
  lv_obj_t *btn = lv_btn_create(_parent);
  lv_obj_set_height(btn, 46);
  lv_obj_set_style_bg_color(btn, _bg, 0);
  lv_obj_set_style_radius(btn, 8, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, _text);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_center(label);
  return btn;
}

static lv_obj_t *ui_create_textarea(UI *_UI, lv_obj_t *parent,
                                    const char *placeholder) {
  lv_obj_t *ta = lv_textarea_create(parent);

  lv_obj_set_width(ta, LV_PCT(100));
  lv_obj_set_height(ta, 46);
  lv_textarea_set_placeholder_text(ta, placeholder);

  lv_obj_add_event_cb(ta, textarea_event_cb, LV_EVENT_FOCUSED, _UI);

  return ta;
}

static lv_obj_t *ui_create_form_field(UI *_UI, lv_obj_t *_parent,
                                      const char *_label,
                                      const char *_placeholder) {
  ui_create_label(_parent, _label, lv_color_hex(0xD1D5DB));
  return ui_create_textarea(_UI, _parent, _placeholder);
}

static void ui_create_metric_card(lv_obj_t *_parent, const char *_title,
                                  const char *_value, const char *_sub,
                                  lv_color_t _accent) {
  lv_obj_t *card = lv_obj_create(_parent);
  lv_obj_set_size(card, LV_PCT(100), 105);
  lv_obj_set_style_bg_color(card, lv_color_hex(C_CARD), 0);
  lv_obj_set_style_border_color(card, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 10, 0);
  lv_obj_set_style_pad_all(card, 14, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *t = ui_create_label(card, _title, lv_color_hex(C_MUTED));
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t *v = ui_create_label(card, _value, lv_color_white());
  lv_obj_set_style_text_color(v, _accent, 0);
  lv_obj_align(v, LV_ALIGN_TOP_LEFT, 0, 30);
  lv_obj_t *s = ui_create_label(card, _sub, lv_color_hex(C_MUTED));
  lv_obj_align(s, LV_ALIGN_TOP_LEFT, 0, 62);
}

static void ui_build_screen_home(UI *_UI) {
  _UI->screen_home = lv_obj_create(_UI->content);
  _UI->active_screen = _UI->screen_home;
  lv_obj_set_size(_UI->screen_home, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(_UI->screen_home, lv_color_hex(C_BLACK), 0);
  lv_obj_set_style_border_width(_UI->screen_home, 0, 0);
  lv_obj_set_style_pad_all(_UI->screen_home, 24, 0);
  lv_obj_set_style_pad_gap(_UI->screen_home, 18, 0);
  lv_obj_set_layout(_UI->screen_home, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->screen_home, LV_FLEX_FLOW_COLUMN);

  lv_obj_t *title =
      ui_create_label(_UI->screen_home, "ESPMaestro", lv_color_white());
  lv_obj_set_style_text_font(title, &notosans_14, 0);
  lv_obj_t *subtitle = ui_create_label(
      _UI->screen_home, "Local energy overview", lv_color_hex(C_MUTED));
  (void)subtitle;

  lv_obj_t *grid = lv_obj_create(_UI->screen_home);
  lv_obj_set_width(grid, LV_PCT(100));
  lv_obj_set_height(grid, 245);
  lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_style_pad_gap(grid, 16, 0);
  lv_obj_set_layout(grid, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW);

  lv_obj_t *col1 = lv_obj_create(grid);
  lv_obj_set_size(col1, LV_PCT(33), LV_PCT(100));
  lv_obj_set_flex_grow(col1, 1);
  lv_obj_t *col2 = lv_obj_create(grid);
  lv_obj_set_size(col2, LV_PCT(33), LV_PCT(100));
  lv_obj_set_flex_grow(col2, 1);
  lv_obj_t *col3 = lv_obj_create(grid);
  lv_obj_set_size(col3, LV_PCT(33), LV_PCT(100));
  lv_obj_set_flex_grow(col3, 1);
  lv_obj_t *cols[] = {col1, col2, col3};
  for (int i = 0; i < 3; i++) {
    lv_obj_set_style_bg_opa(cols[i], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cols[i], 0, 0);
    lv_obj_set_style_pad_all(cols[i], 0, 0);
    lv_obj_set_style_pad_gap(cols[i], 14, 0);
    lv_obj_set_layout(cols[i], LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cols[i], LV_FLEX_FLOW_COLUMN);
  }

  ui_create_metric_card(col1, "Time", "12:45", "2026-04-28",
                        lv_color_hex(C_BLUE));
  ui_create_metric_card(col1, "Status", "System OK", _UI->wifi_status,
                        lv_color_hex(C_GREEN));
  ui_create_metric_card(col2, "Outdoor", "8°C", "Cloudy, light wind",
                        lv_color_hex(C_BLUE));
  ui_create_metric_card(col2, "Forecast", "Low solar", "Updated recently",
                        lv_color_hex(C_YELLOW));
  ui_create_metric_card(col3, "Current price", "68 öre/kWh",
                        "Normal price level", lv_color_hex(C_YELLOW));
  ui_create_metric_card(col3, "Meter", "Waiting", "P1/HAN data not wired",
                        lv_color_hex(C_PURPLE));
}

static void ui_fill_chart(lv_obj_t *_chart, const int *_values, int _count,
                          lv_color_t _color) {
  lv_chart_set_type(_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(_chart, _count);
  lv_chart_set_range(_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_div_line_count(_chart, 5, 8);
  lv_obj_set_style_bg_color(_chart, lv_color_hex(C_CARD_DARK), 0);
  lv_obj_set_style_border_color(_chart, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_border_width(_chart, 1, 0);
  lv_obj_set_style_radius(_chart, 8, 0);
  lv_chart_series_t *ser =
      lv_chart_add_series(_chart, _color, LV_CHART_AXIS_PRIMARY_Y);
  for (int i = 0; i < _count; i++)
    lv_chart_set_next_value(_chart, ser, _values[i]);
}

static void ui_build_screen_forecast(UI *_UI) {
  static const int temp[] = {2, 5, 8, 12, 16, 14, 10, 6};
  const char *rows[][3] = {{"00:00", "2°C", "10%"},  {"03:00", "5°C", "20%"},
                           {"06:00", "8°C", "40%"},  {"09:00", "12°C", "65%"},
                           {"12:00", "16°C", "80%"}, {"15:00", "14°C", "60%"},
                           {"18:00", "10°C", "25%"}, {"21:00", "6°C", "5%"}};
  _UI->screen_forecast = lv_obj_create(_UI->content);
  _UI->active_screen = _UI->screen_forecast;
  lv_obj_clear_flag(_UI->screen_forecast, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->screen_forecast, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_size(_UI->screen_forecast, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(_UI->screen_forecast, lv_color_hex(C_BLACK), 0);
  lv_obj_set_style_border_width(_UI->screen_forecast, 0, 0);
  lv_obj_set_style_pad_all(_UI->screen_forecast, 24, 0);
  lv_obj_clear_flag(_UI->screen_forecast, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->screen_forecast, LV_SCROLLBAR_MODE_OFF);
  lv_obj_t *panel = ui_create_panel(_UI->screen_forecast);
  lv_obj_set_size(panel, 880, 430);
  lv_obj_center(panel);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
  lv_obj_t *title = ui_create_label(panel, "Forecast", lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
  _UI->forecast_table_btn =
      ui_create_button(panel, "Table", lv_color_hex(C_CARD));
  lv_obj_set_size(_UI->forecast_table_btn, 86, 38);
  lv_obj_align(_UI->forecast_table_btn, LV_ALIGN_BOTTOM_RIGHT, -96, 0);
  lv_obj_add_event_cb(_UI->forecast_table_btn, view_toggle_event_cb,
                      LV_EVENT_CLICKED, _UI);

  _UI->forecast_graph_btn =
      ui_create_button(panel, "Graph", lv_color_hex(C_BLUE));
  lv_obj_set_size(_UI->forecast_graph_btn, 86, 38);
  lv_obj_align(_UI->forecast_graph_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(_UI->forecast_graph_btn, view_toggle_event_cb,
                      LV_EVENT_CLICKED, _UI);
  lv_obj_t *forecast_view_box = lv_obj_create(panel);
  lv_obj_set_size(forecast_view_box, LV_PCT(100), 235);
  lv_obj_align(forecast_view_box, LV_ALIGN_TOP_LEFT, 0, 66);
  lv_obj_set_style_bg_opa(forecast_view_box, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(forecast_view_box, 0, 0);
  lv_obj_set_style_pad_all(forecast_view_box, 0, 0);
  lv_obj_clear_flag(forecast_view_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(forecast_view_box, LV_SCROLLBAR_MODE_OFF);

  _UI->forecast_chart = lv_chart_create(forecast_view_box);
  lv_obj_set_size(_UI->forecast_chart, LV_PCT(100), 220);
  lv_obj_align(_UI->forecast_chart, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_clear_flag(_UI->forecast_chart, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->forecast_chart, LV_SCROLLBAR_MODE_OFF);
  ui_fill_chart(_UI->forecast_chart, temp, 8, lv_color_hex(C_BLUE));

  _UI->forecast_table = lv_table_create(forecast_view_box);
  lv_obj_set_size(_UI->forecast_table, LV_PCT(100), 220);
  lv_obj_align(_UI->forecast_table, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_clear_flag(_UI->forecast_table, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->forecast_table, LV_SCROLLBAR_MODE_OFF);
  lv_table_set_col_cnt(_UI->forecast_table, 3);
  lv_table_set_row_cnt(_UI->forecast_table, 9);
  lv_table_set_cell_value(_UI->forecast_table, 0, 0, "Time");
  lv_table_set_cell_value(_UI->forecast_table, 0, 1, "Temp");
  lv_table_set_cell_value(_UI->forecast_table, 0, 2, "Solar");
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 3; j++) {
      lv_table_set_cell_value(_UI->forecast_table, i + 1, j, rows[i][j]);
    }
  }
  lv_obj_add_flag(_UI->forecast_table, LV_OBJ_FLAG_HIDDEN);
}

static void ui_build_screen_elpriser(UI *_UI) {
  static const int price[] = {45, 52, 68, 85, 95, 78, 68, 55, 42};
  const char *rows[][3] = {
      {"00:00", "45", "0"},  {"03:00", "52", "0"},  {"06:00", "68", "20"},
      {"09:00", "85", "60"}, {"12:00", "95", "85"}, {"15:00", "78", "70"},
      {"18:00", "68", "30"}, {"21:00", "55", "5"},  {"24:00", "42", "0"}};
  _UI->screen_elpriser = lv_obj_create(_UI->content);
  _UI->active_screen = _UI->screen_elpriser;
  lv_obj_clear_flag(_UI->screen_elpriser, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->screen_elpriser, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_size(_UI->screen_elpriser, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(_UI->screen_elpriser, lv_color_hex(C_BLACK), 0);
  lv_obj_set_style_border_width(_UI->screen_elpriser, 0, 0);
  lv_obj_set_style_pad_all(_UI->screen_elpriser, 24, 0);
  lv_obj_clear_flag(_UI->screen_elpriser, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->screen_elpriser, LV_SCROLLBAR_MODE_OFF);
  lv_obj_t *panel = ui_create_panel(_UI->screen_elpriser);
  lv_obj_set_size(panel, 880, 430);
  lv_obj_center(panel);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
  lv_obj_t *title = ui_create_label(panel, "Elpriser", lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
  _UI->elpriser_table_btn =
      ui_create_button(panel, "Table", lv_color_hex(C_CARD));
  lv_obj_set_size(_UI->elpriser_table_btn, 86, 38);
  lv_obj_align(_UI->elpriser_table_btn, LV_ALIGN_BOTTOM_RIGHT, -96, 0);
  lv_obj_add_event_cb(_UI->elpriser_table_btn, view_toggle_event_cb,
                      LV_EVENT_CLICKED, _UI);

  _UI->elpriser_graph_btn =
      ui_create_button(panel, "Graph", lv_color_hex(C_BLUE));
  lv_obj_set_size(_UI->elpriser_graph_btn, 86, 38);
  lv_obj_align(_UI->elpriser_graph_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(_UI->elpriser_graph_btn, view_toggle_event_cb,
                      LV_EVENT_CLICKED, _UI);
  lv_obj_t *elpriser_view_box = lv_obj_create(panel);
  lv_obj_set_size(elpriser_view_box, LV_PCT(100), 235);
  lv_obj_align(elpriser_view_box, LV_ALIGN_TOP_LEFT, 0, 66);
  lv_obj_set_style_bg_opa(elpriser_view_box, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(elpriser_view_box, 0, 0);
  lv_obj_set_style_pad_all(elpriser_view_box, 0, 0);
  lv_obj_clear_flag(elpriser_view_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(elpriser_view_box, LV_SCROLLBAR_MODE_OFF);

  _UI->elpriser_chart = lv_chart_create(elpriser_view_box);
  lv_obj_set_size(_UI->elpriser_chart, LV_PCT(100), 220);
  lv_obj_align(_UI->elpriser_chart, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_clear_flag(_UI->elpriser_chart, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->elpriser_chart, LV_SCROLLBAR_MODE_OFF);
  ui_fill_chart(_UI->elpriser_chart, price, 9, lv_color_hex(C_YELLOW));

  _UI->elpriser_table = lv_table_create(elpriser_view_box);
  lv_obj_set_size(_UI->elpriser_table, LV_PCT(100), 220);
  lv_obj_align(_UI->elpriser_table, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_clear_flag(_UI->elpriser_table, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->elpriser_table, LV_SCROLLBAR_MODE_OFF);
  lv_table_set_col_cnt(_UI->elpriser_table, 3);
  lv_table_set_row_cnt(_UI->elpriser_table, 10);
  lv_table_set_cell_value(_UI->elpriser_table, 0, 0, "Tid");
  lv_table_set_cell_value(_UI->elpriser_table, 0, 1, "Pris");
  lv_table_set_cell_value(_UI->elpriser_table, 0, 2, "Sol %");
  for (int i = 0; i < 9; i++) {
    for (int j = 0; j < 3; j++) {
      lv_table_set_cell_value(_UI->elpriser_table, i + 1, j, rows[i][j]);
    }
  }
  lv_obj_add_flag(_UI->elpriser_table, LV_OBJ_FLAG_HIDDEN);
}

static void ui_create_settings_card(UI *_UI, lv_obj_t *_parent, lv_obj_t **_out,
                                    const char *_title, const char *_sub,
                                    lv_color_t _accent) {
  *_out = lv_btn_create(_parent);
  lv_obj_set_size(*_out, LV_PCT(100), 96);
  lv_obj_set_style_bg_color(*_out, lv_color_hex(C_CARD), 0);
  lv_obj_set_style_border_color(*_out, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_border_width(*_out, 1, 0);
  lv_obj_set_style_radius(*_out, 10, 0);
  lv_obj_set_style_shadow_width(*_out, 0, 0);
  lv_obj_add_event_cb(*_out, settings_card_event_cb, LV_EVENT_CLICKED, _UI);
  lv_obj_t *accent = lv_obj_create(*_out);
  lv_obj_set_size(accent, 54, 54);
  lv_obj_align(accent, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_set_style_bg_color(accent, _accent, 0);
  lv_obj_set_style_radius(accent, 10, 0);
  lv_obj_set_style_border_width(accent, 0, 0);
  lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *t = ui_create_label(*_out, _title, lv_color_white());
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 78, -14);
  lv_obj_t *s = ui_create_label(*_out, _sub, lv_color_hex(C_MUTED));
  lv_obj_align(s, LV_ALIGN_LEFT_MID, 78, 16);
}

static void ui_build_screen_settings(UI *_UI) {
  _UI->screen_settings = lv_obj_create(_UI->content);
  _UI->active_screen = _UI->screen_settings;
  lv_obj_clear_flag(_UI->screen_settings, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->screen_settings, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_size(_UI->screen_settings, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(_UI->screen_settings, lv_color_hex(C_BLACK), 0);
  lv_obj_set_style_border_width(_UI->screen_settings, 0, 0);
  lv_obj_set_style_pad_all(_UI->screen_settings, 32, 0);
  lv_obj_t *panel = ui_create_panel(_UI->screen_settings);
  lv_obj_set_size(panel, 820, 420);
  lv_obj_center(panel);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(panel, 16, 0);
  ui_create_label(panel, "Settings", lv_color_white());
  ui_create_settings_card(_UI, panel, &_UI->card_wifi, "WiFi Configuration",
                          "Configure network settings", lv_color_hex(C_BLUE));
  ui_create_settings_card(_UI, panel, &_UI->card_facility,
                          "Facility Configuration",
                          "Configure facility details", lv_color_hex(C_GREEN));
  ui_create_settings_card(_UI, panel, &_UI->card_device_info,
                          "Device Information", "Hardware specifications",
                          lv_color_hex(C_PURPLE));
}

static void ui_build_screen_wifi(UI *_UI) {
  _UI->screen_wifi = lv_obj_create(_UI->content);
  _UI->active_screen = _UI->screen_wifi;
  lv_obj_clear_flag(_UI->screen_wifi, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->screen_wifi, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_size(_UI->screen_wifi, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(_UI->screen_wifi, lv_color_hex(C_BLACK), 0);
  lv_obj_set_style_border_width(_UI->screen_wifi, 0, 0);
  lv_obj_set_style_pad_all(_UI->screen_wifi, 24, 0);
  lv_obj_clear_flag(_UI->screen_wifi, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->screen_wifi, LV_SCROLLBAR_MODE_OFF);
  lv_obj_t *panel = ui_create_panel(_UI->screen_wifi);
  lv_obj_set_size(panel, 860, 420);
  lv_obj_center(panel);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
  _UI->back_btn =
      ui_create_button(panel, LV_SYMBOL_LEFT, lv_color_hex(0x374151));
  lv_obj_set_size(_UI->back_btn, 48, 48);
  lv_obj_align(_UI->back_btn, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_add_event_cb(_UI->back_btn, back_event_cb, LV_EVENT_CLICKED, _UI);
  lv_obj_t *title =
      ui_create_label(panel, "WiFi Configuration", lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 70, 0);
  lv_obj_t *sub = ui_create_label(panel, "Configure network settings",
                                  lv_color_hex(C_MUTED));
  lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 70, 28);
  lv_obj_t *connected = lv_obj_create(panel);
  lv_obj_set_size(connected, LV_PCT(100), 62);
  lv_obj_align(connected, LV_ALIGN_TOP_LEFT, 0, 62);
  lv_obj_set_style_bg_color(connected, lv_color_hex(C_CARD), 0);
  lv_obj_set_style_border_color(connected, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_border_width(connected, 1, 0);
  lv_obj_set_style_radius(connected, 8, 0);
  lv_obj_set_style_pad_all(connected, 12, 0);
  lv_obj_clear_flag(connected, LV_OBJ_FLAG_SCROLLABLE);
  char buf[160];
  snprintf(buf, sizeof(buf), "Connected: %s\nIP Address: %s",
           _UI->wifi_connected ? _UI->wifi_ssid : "Not connected",
           _UI->wifi_connected ? _UI->wifi_ip : "-");
  ui_create_label(connected, buf, lv_color_white());
  lv_obj_t *net_title =
      ui_create_label(panel, "Available Networks", lv_color_white());
  lv_obj_align(net_title, LV_ALIGN_TOP_LEFT, 0, 136);
  _UI->wifi_scan_btn = ui_create_button(panel, "Scan", lv_color_hex(C_BLUE));
  lv_obj_set_size(_UI->wifi_scan_btn, 100, 36);
  lv_obj_align(_UI->wifi_scan_btn, LV_ALIGN_TOP_RIGHT, 0, 128);
  lv_obj_add_event_cb(_UI->wifi_scan_btn, wifi_scan_btn_event_cb,
                      LV_EVENT_CLICKED, _UI);
  lv_obj_t *list = lv_obj_create(panel);
  lv_obj_set_size(list, LV_PCT(100), 150);
  lv_obj_align(list, LV_ALIGN_TOP_LEFT, 0, 174);
  lv_obj_set_style_bg_color(list, lv_color_hex(C_CARD), 0);
  lv_obj_set_style_border_color(list, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_border_width(list, 1, 0);
  lv_obj_set_style_radius(list, 8, 0);
  lv_obj_set_style_pad_all(list, 8, 0);
  lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  for (int i = 0; i < 4; i++) {
    _UI->wifi_network_rows[i] = lv_btn_create(list);
    lv_obj_set_size(_UI->wifi_network_rows[i], LV_PCT(100), 30);
    lv_obj_set_pos(_UI->wifi_network_rows[i], 0, i * 34);
    lv_obj_set_style_bg_color(_UI->wifi_network_rows[i], lv_color_hex(C_PANEL),
                              0);
    lv_obj_set_style_radius(_UI->wifi_network_rows[i], 7, 0);
    lv_obj_set_style_shadow_width(_UI->wifi_network_rows[i], 0, 0);
    lv_obj_set_user_data(_UI->wifi_network_rows[i], (void *)(uintptr_t)i);
    lv_obj_add_event_cb(_UI->wifi_network_rows[i], wifi_network_row_event_cb,
                        LV_EVENT_CLICKED, _UI);
    _UI->wifi_network_labels[i] =
        ui_create_label(_UI->wifi_network_rows[i], "Scan for networks...",
                        lv_color_hex(C_MUTED));
    lv_obj_align(_UI->wifi_network_labels[i], LV_ALIGN_LEFT_MID, 8, 0);
  }
  _UI->wifi_prev_btn = ui_create_button(panel, "Prev", lv_color_hex(0x374151));
  lv_obj_set_size(_UI->wifi_prev_btn, 76, 34);
  lv_obj_align(_UI->wifi_prev_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_event_cb(_UI->wifi_prev_btn, wifi_prev_page_event_cb,
                      LV_EVENT_CLICKED, _UI);
  _UI->wifi_page_label = ui_create_label(panel, "1 / 1", lv_color_hex(C_MUTED));
  lv_obj_align(_UI->wifi_page_label, LV_ALIGN_BOTTOM_MID, 0, -8);
  _UI->wifi_next_btn = ui_create_button(panel, "Next", lv_color_hex(0x374151));
  lv_obj_set_size(_UI->wifi_next_btn, 76, 34);
  lv_obj_align(_UI->wifi_next_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(_UI->wifi_next_btn, wifi_next_page_event_cb,
                      LV_EVENT_CLICKED, _UI);
  _UI->wifi_status_label = ui_create_label(panel, "", lv_color_white());
  lv_obj_align(_UI->wifi_status_label, LV_ALIGN_BOTTOM_MID, 0, -38);
  ui_update_wifi_rows(_UI);

  if (_UI->wifi_network_count <= 0) {
    lv_obj_add_flag(list, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_UI->wifi_prev_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_UI->wifi_next_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_UI->wifi_page_label, LV_OBJ_FLAG_HIDDEN);
  }
}

static void ui_build_facility_page(UI *_UI) {
  if (!_UI || !_UI->facility_form)
    return;

  lv_obj_clean(_UI->facility_form);

  lv_obj_set_layout(_UI->facility_form, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->facility_form, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(_UI->facility_form, 8, 0);

  if (_UI->facility_page == 0) {
    _UI->facility_name_ta = ui_create_form_field(
        _UI, _UI->facility_form, "Facility Name", "Enter facility name");

    _UI->facility_address_ta = ui_create_form_field(
        _UI, _UI->facility_form, "Address", "Street address");

    lv_textarea_set_text(_UI->facility_name_ta,
                         _UI->facility_cfg.facility_name);
    lv_textarea_set_text(_UI->facility_address_ta, _UI->facility_cfg.address);

  } else if (_UI->facility_page == 1) {
    _UI->facility_city_ta =
        ui_create_form_field(_UI, _UI->facility_form, "City", "City");

    _UI->facility_zip_ta =
        ui_create_form_field(_UI, _UI->facility_form, "ZIP", "ZIP");

    char zip_buf[8];
    snprintf(zip_buf, sizeof(zip_buf), "%u", _UI->facility_cfg.zip);

    lv_textarea_set_text(_UI->facility_city_ta, _UI->facility_cfg.city);
    lv_textarea_set_text(_UI->facility_zip_ta, zip_buf);

  } else {
    _UI->facility_lat_ta =
        ui_create_form_field(_UI, _UI->facility_form, "Latitude", "59.3293");

    _UI->facility_lon_ta =
        ui_create_form_field(_UI, _UI->facility_form, "Longitude", "18.0686");

    _UI->facility_energy_zone_ta =
        ui_create_form_field(_UI, _UI->facility_form, "Energy Zone", "1-4");

    char zone_buf[8];
    snprintf(zone_buf, sizeof(zone_buf), "%u", _UI->facility_cfg.energy_zone);

    lv_textarea_set_text(_UI->facility_lat_ta, _UI->facility_cfg.lat);
    lv_textarea_set_text(_UI->facility_lon_ta, _UI->facility_cfg.lon);
    lv_textarea_set_text(_UI->facility_energy_zone_ta, zone_buf);
  }
}

static void ui_build_screen_facility(UI *_UI) {
  _UI->screen_facility = lv_obj_create(_UI->content);
  _UI->active_screen = _UI->screen_facility;
  lv_obj_clear_flag(_UI->screen_facility, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->screen_facility, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_size(_UI->screen_facility, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(_UI->screen_facility, lv_color_hex(C_BLACK), 0);
  lv_obj_set_style_border_width(_UI->screen_facility, 0, 0);
  lv_obj_set_style_pad_all(_UI->screen_facility, 32, 0);
  lv_obj_t *panel = ui_create_panel(_UI->screen_facility);
  lv_obj_set_size(panel, 820, 420);
  lv_obj_center(panel);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
  _UI->back_btn =
      ui_create_button(panel, LV_SYMBOL_LEFT, lv_color_hex(0x374151));
  lv_obj_set_size(_UI->back_btn, 48, 48);
  lv_obj_align(_UI->back_btn, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_add_event_cb(_UI->back_btn, back_event_cb, LV_EVENT_CLICKED, _UI);
  lv_obj_t *title =
      ui_create_label(panel, "Facility Configuration", lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 70, 0);
  _UI->facility_form = lv_obj_create(panel);
  lv_obj_set_size(_UI->facility_form, LV_PCT(100), 260);
  lv_obj_align(_UI->facility_form, LV_ALIGN_TOP_LEFT, 0, 62);
  lv_obj_set_style_bg_opa(_UI->facility_form, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(_UI->facility_form, 0, 0);
  lv_obj_set_style_pad_all(_UI->facility_form, 0, 0);
  lv_obj_clear_flag(_UI->facility_form, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->facility_form, LV_SCROLLBAR_MODE_OFF);
  _UI->facility_page = 0;

  esp_err_t err = facility_config_load(&_UI->facility_cfg);
  if (err != ESP_OK) {
    memset(&_UI->facility_cfg, 0, sizeof(_UI->facility_cfg));
  }

  ui_build_facility_page(_UI);

  lv_obj_t *prev = ui_create_button(panel, "Prev", lv_color_hex(0x374151));
  lv_obj_set_size(prev, 90, 42);
  lv_obj_align(prev, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_event_cb(prev, facility_prev_event_cb, LV_EVENT_CLICKED, _UI);
  _UI->facility_save_btn =
      ui_create_button(panel, "Save", lv_color_hex(C_GREEN));
  lv_obj_set_size(_UI->facility_save_btn, 120, 42);
  lv_obj_align(_UI->facility_save_btn, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_event_cb(_UI->facility_save_btn, facility_save_event_cb,
                      LV_EVENT_CLICKED, _UI);
  lv_obj_t *next = ui_create_button(panel, "Next", lv_color_hex(0x374151));
  lv_obj_set_size(next, 90, 42);
  lv_obj_align(next, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(next, facility_next_event_cb, LV_EVENT_CLICKED, _UI);
  _UI->facility_status_label =
      ui_create_label(panel, "", lv_color_hex(C_MUTED));
  lv_obj_align(_UI->facility_status_label, LV_ALIGN_BOTTOM_MID, 0, -48);
}

static void ui_build_screen_device_info(UI *_UI) {
  const char *specs[][2] = {{"Device Name", "ESP32-S3-Display"},
                            {"Chip Model", "ESP32-S3"},
                            {"CPU Frequency", "240 MHz"},
                            {"Flash Size", "16 MB"},
                            {"RAM Size", "512 KB"},
                            {"PSRAM", "8 MB"},
                            {"Display", "1024x600 7\""},
                            {"WiFi", "802.11 b/g/n"},
                            {"Bluetooth", "BLE 5.0"},
                            {"Temperature", "42°C"}};
  _UI->screen_device_info = lv_obj_create(_UI->content);
  _UI->active_screen = _UI->screen_device_info;
  lv_obj_clear_flag(_UI->screen_device_info, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->screen_device_info, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_size(_UI->screen_device_info, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(_UI->screen_device_info, lv_color_hex(C_BLACK), 0);
  lv_obj_set_style_border_width(_UI->screen_device_info, 0, 0);
  lv_obj_set_style_pad_all(_UI->screen_device_info, 32, 0);
  lv_obj_t *panel = ui_create_panel(_UI->screen_device_info);
  lv_obj_set_size(panel, 850, 420);
  lv_obj_center(panel);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
  _UI->back_btn =
      ui_create_button(panel, LV_SYMBOL_LEFT, lv_color_hex(0x374151));
  lv_obj_set_size(_UI->back_btn, 48, 48);
  lv_obj_align(_UI->back_btn, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_add_event_cb(_UI->back_btn, back_event_cb, LV_EVENT_CLICKED, _UI);
  lv_obj_t *title =
      ui_create_label(panel, "Device Information", lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 70, 0);
  lv_obj_t *sub =
      ui_create_label(panel, "Hardware specifications", lv_color_hex(C_MUTED));
  lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 70, 28);
  for (int i = 0; i < 10; i++) {
    lv_obj_t *card = lv_obj_create(panel);
    lv_obj_set_size(card, 390, 50);
    lv_obj_set_pos(card, (i % 2) * 410, 70 + (i / 2) * 58);
    lv_obj_set_style_bg_color(card, lv_color_hex(C_CARD), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = ui_create_label(card, specs[i][0], lv_color_hex(C_MUTED));
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 12, -10);
    lv_obj_t *v = ui_create_label(card, specs[i][1], lv_color_white());
    lv_obj_align(v, LV_ALIGN_LEFT_MID, 12, 12);
  }
}

static void nav_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  lv_obj_t *btn = lv_event_get_target(_event);
  if (!_UI || !btn)
    return;
  ui_show_screen(_UI, (UI_Screen)(uintptr_t)lv_obj_get_user_data(btn));
}
static void back_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  if (_UI)
    ui_show_screen(_UI, UI_SCREEN_SETTINGS);
}
static void settings_card_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  lv_obj_t *target = lv_event_get_target(_event);
  if (!_UI)
    return;
  if (target == _UI->card_wifi)
    ui_show_screen(_UI, UI_SCREEN_WIFI);
  else if (target == _UI->card_facility)
    ui_show_screen(_UI, UI_SCREEN_FACILITY);
  else if (target == _UI->card_device_info)
    ui_show_screen(_UI, UI_SCREEN_DEVICE_INFO);
}

static void view_toggle_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  lv_obj_t *target = lv_event_get_target(_event);
  if (!_UI || !target)
    return;
  if (target == _UI->forecast_graph_btn || target == _UI->forecast_table_btn) {
    bool table = target == _UI->forecast_table_btn;
    if (table) {
      lv_obj_add_flag(_UI->forecast_chart, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(_UI->forecast_table, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_color(_UI->forecast_table_btn, lv_color_hex(C_BLUE),
                                0);
      lv_obj_set_style_bg_color(_UI->forecast_graph_btn, lv_color_hex(C_CARD),
                                0);
    } else {
      lv_obj_clear_flag(_UI->forecast_chart, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(_UI->forecast_table, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_color(_UI->forecast_graph_btn, lv_color_hex(C_BLUE),
                                0);
      lv_obj_set_style_bg_color(_UI->forecast_table_btn, lv_color_hex(C_CARD),
                                0);
    }
  } else if (target == _UI->elpriser_graph_btn ||
             target == _UI->elpriser_table_btn) {
    bool table = target == _UI->elpriser_table_btn;
    if (table) {
      lv_obj_add_flag(_UI->elpriser_chart, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(_UI->elpriser_table, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_color(_UI->elpriser_table_btn, lv_color_hex(C_BLUE),
                                0);
      lv_obj_set_style_bg_color(_UI->elpriser_graph_btn, lv_color_hex(C_CARD),
                                0);
    } else {
      lv_obj_clear_flag(_UI->elpriser_chart, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(_UI->elpriser_table, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_color(_UI->elpriser_graph_btn, lv_color_hex(C_BLUE),
                                0);
      lv_obj_set_style_bg_color(_UI->elpriser_table_btn, lv_color_hex(C_CARD),
                                0);
    }
  }
}

static void ui_update_wifi_rows(UI *_UI) {
  if (!_UI) {
    return;
  }

  int total_pages = (_UI->wifi_network_count + 3) / 4;

  if (total_pages < 1) {
    total_pages = 1;
  }

  if (_UI->wifi_network_page >= total_pages) {
    _UI->wifi_network_page = total_pages - 1;
  }
  for (int i = 0; i < 4; i++) {
    if (!_UI->wifi_network_labels[i])
      continue;
    int idx = _UI->wifi_network_page * 4 + i;
    if (idx < _UI->wifi_network_count) {
      lv_label_set_text(_UI->wifi_network_labels[i], _UI->wifi_networks[idx]);
      lv_obj_clear_state(_UI->wifi_network_rows[i], LV_STATE_DISABLED);
      lv_obj_set_style_text_color(_UI->wifi_network_labels[i], lv_color_white(),
                                  0);
    } else {
      lv_label_set_text(_UI->wifi_network_labels[i],
                        _UI->wifi_network_count ? "" : "Scan for networks...");
      lv_obj_add_state(_UI->wifi_network_rows[i], LV_STATE_DISABLED);
      lv_obj_set_style_text_color(_UI->wifi_network_labels[i],
                                  lv_color_hex(C_MUTED), 0);
    }
  }
  if (_UI->wifi_page_label) {
    char b[24];
    snprintf(b, sizeof(b), "%d / %d", _UI->wifi_network_page + 1, total_pages);
    lv_label_set_text(_UI->wifi_page_label, b);
  }
}

static void wifi_scan_btn_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  if (!_UI)
    return;
  ui_set_wifi_form_status(_UI, "Scanning...", false);
  if (wifi_handler_scan() != ESP_OK)
    ui_set_wifi_form_status(_UI, "Failed to start scan", true);
}
static void wifi_network_row_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  lv_obj_t *row = lv_event_get_target(_event);
  if (!_UI || !row)
    return;
  int local = (int)(uintptr_t)lv_obj_get_user_data(row);
  int idx = _UI->wifi_network_page * 4 + local;
  if (idx >= 0 && idx < _UI->wifi_network_count)
    ui_open_wifi_password(_UI, idx);
}
static void wifi_prev_page_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  if (_UI && _UI->wifi_network_page > 0) {
    _UI->wifi_network_page--;
    ui_update_wifi_rows(_UI);
  }
}
static void wifi_next_page_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  if (!_UI)
    return;
  int pages = (_UI->wifi_network_count + 3) / 4;
  if (pages < 1)
    pages = 1;
  if (_UI->wifi_network_page < pages - 1) {
    _UI->wifi_network_page++;
    ui_update_wifi_rows(_UI);
  }
}

static void ui_open_wifi_password(UI *_UI, int _idx) {
  _UI->wifi_network_selected = _idx;
  _UI->wifi_connecting_index = _idx;

  _UI->wifi_password_overlay = lv_obj_create(_UI->screen_wifi);
  lv_obj_set_size(_UI->wifi_password_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(_UI->wifi_password_overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(_UI->wifi_password_overlay, LV_OPA_70, 0);
  lv_obj_set_style_border_width(_UI->wifi_password_overlay, 0, 0);
  lv_obj_clear_flag(_UI->wifi_password_overlay, LV_OBJ_FLAG_SCROLLABLE);

  _UI->wifi_password_panel = lv_obj_create(_UI->wifi_password_overlay);
  lv_obj_set_size(_UI->wifi_password_panel, 660, 220);
  lv_obj_align(_UI->wifi_password_panel, LV_ALIGN_TOP_MID, 0, 90);
  lv_obj_set_style_bg_color(_UI->wifi_password_panel, lv_color_hex(C_PANEL), 0);
  lv_obj_set_style_border_color(_UI->wifi_password_panel,
                                lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_border_width(_UI->wifi_password_panel, 2, 0);
  lv_obj_set_style_radius(_UI->wifi_password_panel, 10, 0);
  lv_obj_set_style_pad_all(_UI->wifi_password_panel, 20, 0);

  char title[96];
  snprintf(title, sizeof(title), "Connect to: %s", _UI->wifi_networks[_idx]);
  ui_create_label(_UI->wifi_password_panel, title, lv_color_white());

  _UI->wifi_pass_ta =
      ui_create_textarea(_UI, _UI->wifi_password_panel, "Enter password");

  lv_obj_set_size(_UI->wifi_pass_ta, LV_PCT(100), 48);
  lv_obj_align(_UI->wifi_pass_ta, LV_ALIGN_TOP_LEFT, 0, 50);
  lv_textarea_set_password_mode(_UI->wifi_pass_ta, true);

  lv_obj_t *cancel = ui_create_button(_UI->wifi_password_panel, "Cancel",
                                      lv_color_hex(0x374151));
  lv_obj_set_size(cancel, 120, 42);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_event_cb(cancel, wifi_cancel_event_cb, LV_EVENT_CLICKED, _UI);

  _UI->wifi_connect_btn = ui_create_button(_UI->wifi_password_panel, "Connect",
                                           lv_color_hex(C_BLUE));
  lv_obj_set_size(_UI->wifi_connect_btn, 120, 42);
  lv_obj_align(_UI->wifi_connect_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(_UI->wifi_connect_btn, connect_event_cb, LV_EVENT_CLICKED,
                      _UI);

  if (_UI->keyboard) {
    lv_keyboard_set_textarea(_UI->keyboard, _UI->wifi_pass_ta);
    lv_obj_clear_flag(_UI->keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(_UI->keyboard);
    lv_obj_add_state(_UI->wifi_pass_ta, LV_STATE_FOCUSED);
  }
}
static void ui_close_wifi_password(UI *_UI) {
  if (!_UI)
    return;

  if (_UI->keyboard) {
    lv_keyboard_set_textarea(_UI->keyboard, NULL);
    lv_obj_add_flag(_UI->keyboard, LV_OBJ_FLAG_HIDDEN);
  }

  if (_UI->wifi_password_overlay) {
    lv_obj_del(_UI->wifi_password_overlay);
    _UI->wifi_password_overlay = NULL;
    _UI->wifi_password_panel = NULL;
    _UI->wifi_pass_ta = NULL;
    _UI->wifi_connect_btn = NULL;
  }
}

static void wifi_cancel_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  ui_close_wifi_password(_UI);
}
static void connect_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  if (!_UI)
    return;
  const char *ssid = _UI->wifi_networks[_UI->wifi_network_selected];
  const char *pass =
      _UI->wifi_pass_ta ? lv_textarea_get_text(_UI->wifi_pass_ta) : "";
  if (!ssid || !ssid[0]) {
    ui_set_wifi_form_status(_UI, "Please scan and select a network", true);
    return;
  }
  ui_set_wifi_busy(_UI, true);
  esp_err_t err = wifi_handler_connect(ssid, pass);
  if (err != ESP_OK) {
    ui_set_wifi_busy(_UI, false);
    ui_set_wifi_form_status(_UI, "Failed to start connection", true);
  }
}

static void facility_prev_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);

  if (_UI && _UI->facility_page > 0) {
    facility_read_current_page(_UI);
    _UI->facility_page--;
    if (_UI->keyboard) {
      lv_keyboard_set_textarea(_UI->keyboard, NULL);
      lv_obj_add_flag(_UI->keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    ui_build_facility_page(_UI);
  }
}

static void facility_next_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);

  if (_UI && _UI->facility_page < 2) {
    facility_read_current_page(_UI);
    _UI->facility_page++;
    if (_UI->keyboard) {
      lv_keyboard_set_textarea(_UI->keyboard, NULL);
      lv_obj_add_flag(_UI->keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    ui_build_facility_page(_UI);
  }
}

static void facility_save_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);

  if (!_UI)
    return;

  facility_read_current_page(_UI);

  if (_UI->keyboard) {
    lv_keyboard_set_textarea(_UI->keyboard, NULL);
    lv_obj_add_flag(_UI->keyboard, LV_OBJ_FLAG_HIDDEN);
  }

  if (_UI->facility_save_btn) {
    lv_obj_add_state(_UI->facility_save_btn, LV_STATE_DISABLED);
  }

  esp_err_t err = facility_config_set_all(&_UI->facility_cfg);

  if (_UI->facility_save_btn) {
    lv_obj_clear_state(_UI->facility_save_btn, LV_STATE_DISABLED);
  }

  if (_UI->facility_status_label) {
    if (err == ESP_OK) {
      lv_label_set_text(_UI->facility_status_label, "Saved facility");
      lv_obj_set_style_text_color(_UI->facility_status_label,
                                  lv_color_hex(C_GREEN), 0);
    } else {
      lv_label_set_text(_UI->facility_status_label, "Failed to save");
      lv_obj_set_style_text_color(_UI->facility_status_label,
                                  lv_color_hex(C_RED), 0);
    }
  }
}

void ui_set_wifi_form_status(UI *_UI, const char *_msg, bool _error) {
  if (!_UI || !_UI->wifi_status_label)
    return;
  lv_label_set_text(_UI->wifi_status_label, _msg ? _msg : "");
  lv_obj_set_style_text_color(_UI->wifi_status_label,
                              _error ? lv_color_hex(C_RED) : lv_color_white(),
                              0);
}
void ui_set_wifi_busy(UI *_UI, bool _busy) {
  if (!_UI)
    return;
  _UI->wifi_connecting = _busy;
  if (_UI->wifi_connect_btn) {
    if (_busy)
      lv_obj_add_state(_UI->wifi_connect_btn, LV_STATE_DISABLED);
    else
      lv_obj_clear_state(_UI->wifi_connect_btn, LV_STATE_DISABLED);
  }
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
  _UI->wifi_connected = _connected;
  snprintf(_UI->wifi_ssid, sizeof(_UI->wifi_ssid), "%s", _ssid ? _ssid : "");
  snprintf(_UI->wifi_ip, sizeof(_UI->wifi_ip), "%s", _ip ? _ip : "");
  if (_connected && _ssid && _ip)
    snprintf(_UI->wifi_status, sizeof(_UI->wifi_status),
             "WiFi OK | SSID: %s | IP: %s", _ssid, _ip);
  else
    snprintf(_UI->wifi_status, sizeof(_UI->wifi_status), "WiFi: Not connected");
  ui_set_footer_text(_UI, _UI->wifi_status);
  ui_update_nav(_UI);
  if (_UI->current_screen == UI_SCREEN_WIFI) {
    ui_close_wifi_password(_UI);
    ui_show_screen(_UI, UI_SCREEN_WIFI);
  }
}
void ui_set_wifi_network_list(UI *_UI, const char *_options) {
  if (!_UI || !_options)
    return;
  memset(_UI->wifi_networks, 0, sizeof(_UI->wifi_networks));
  _UI->wifi_network_count = 0;
  _UI->wifi_network_page = 0;
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
    if (len && strcmp(dst, "Scan for networks...") &&
        strcmp(dst, "No networks found"))
      _UI->wifi_network_count++;
  }
  ui_update_wifi_rows(_UI);

  if (_UI->wifi_network_count > 0) {
    if (_UI->wifi_network_rows[0]) {
      lv_obj_t *list = lv_obj_get_parent(_UI->wifi_network_rows[0]);
      if (list) {
        lv_obj_clear_flag(list, LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (_UI->wifi_prev_btn) {
      lv_obj_clear_flag(_UI->wifi_prev_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (_UI->wifi_next_btn) {
      lv_obj_clear_flag(_UI->wifi_next_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (_UI->wifi_page_label) {
      lv_obj_clear_flag(_UI->wifi_page_label, LV_OBJ_FLAG_HIDDEN);
    }
  }

  ui_set_wifi_form_status(
      _UI, _UI->wifi_network_count > 0 ? "Scan complete" : "No networks found",
      _UI->wifi_network_count == 0);
}

static void ui_hide_keyboard(UI *_UI) {
  if (!_UI || !_UI->keyboard)
    return;

  lv_keyboard_set_textarea(_UI->keyboard, NULL);
  lv_obj_add_flag(_UI->keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void keyboard_event_cb(lv_event_t *e) {
  UI *_UI = lv_event_get_user_data(e);
  lv_event_code_t code = lv_event_get_code(e);

  if (!_UI)
    return;

  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    ui_hide_keyboard(_UI);
  }
}

void ui_set_time(UI *_UI, uint8_t h, uint8_t m, uint8_t s) {
  if (!_UI)
    return;
  char clock[9];
  clock[0] = '0' + (h / 10);
  clock[1] = '0' + (h % 10);
  clock[2] = ':';
  clock[3] = '0' + (m / 10);
  clock[4] = '0' + (m % 10);
  clock[5] = ':';
  clock[6] = '0' + (s / 10);
  clock[7] = '0' + (s % 10);
  clock[8] = '\0';

  if (_UI->nav_clock_label) {
    lv_label_set_text(_UI->nav_clock_label, clock);
  }
}

void ui_set_date(UI *_UI, uint16_t year, uint8_t month, uint8_t day) {
  if (!_UI)
    return;

  char date[11]; // "2026-05-23" + '\0'

  date[0] = '0' + ((year / 1000) % 10);
  date[1] = '0' + ((year / 100) % 10);
  date[2] = '0' + ((year / 10) % 10);
  date[3] = '0' + (year % 10);

  date[4] = '-';

  date[5] = '0' + (month / 10);
  date[6] = '0' + (month % 10);

  date[7] = '-';

  date[8] = '0' + (day / 10);
  date[9] = '0' + (day % 10);

  date[10] = '\0';

  if (_UI->nav_date_label) {
    lv_label_set_text(_UI->nav_date_label, date);
  }
}

void ui_tick(UI *_UI) { (void)_UI; }
