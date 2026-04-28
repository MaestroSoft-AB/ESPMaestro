#ifndef __UI_H_
#define __UI_H_

#include "lvgl.h"
#include <stdbool.h>

typedef enum {
  UI_SCREEN_HOME = 0,
  UI_SCREEN_FORECAST,
  UI_SCREEN_ELPRISER,
  UI_SCREEN_SETTINGS,
  UI_SCREEN_WIFI,
  UI_SCREEN_FACILITY,
  UI_SCREEN_DEVICE_INFO,
} UI_Screen;

typedef enum {
  UI_VIEW_GRAPH = 0,
  UI_VIEW_TABLE,
} UI_ViewMode;

typedef struct {
  lv_obj_t *root;
  lv_obj_t *nav;
  lv_obj_t *content;
  lv_obj_t *footer;
  lv_obj_t *footer_label;

  lv_obj_t *active_screen;

  lv_obj_t *nav_home_btn;
  lv_obj_t *nav_forecast_btn;
  lv_obj_t *nav_elpriser_btn;
  lv_obj_t *nav_settings_btn;
  lv_obj_t *nav_wifi_btn;
  lv_obj_t *wifi_indicator;

  lv_obj_t *screen_home;
  lv_obj_t *screen_forecast;
  lv_obj_t *screen_elpriser;
  lv_obj_t *screen_settings;
  lv_obj_t *screen_wifi;
  lv_obj_t *screen_facility;
  lv_obj_t *screen_device_info;

  lv_obj_t *back_btn;
  lv_obj_t *card_wifi;
  lv_obj_t *card_facility;
  lv_obj_t *card_device_info;

  lv_obj_t *forecast_chart;
  lv_obj_t *forecast_table;
  lv_obj_t *forecast_graph_btn;
  lv_obj_t *forecast_table_btn;
  UI_ViewMode forecast_view_mode;

  lv_obj_t *elpriser_chart;
  lv_obj_t *elpriser_table;
  lv_obj_t *elpriser_graph_btn;
  lv_obj_t *elpriser_table_btn;
  UI_ViewMode elpriser_view_mode;

  lv_obj_t *wifi_pass_ta;
  lv_obj_t *wifi_status_label;
  lv_obj_t *wifi_connect_btn;
  lv_obj_t *wifi_scan_btn;
  lv_obj_t *wifi_keyboard;
  lv_obj_t *wifi_ssid_label;
  lv_obj_t *wifi_network_rows[5];
  lv_obj_t *wifi_network_labels[5];
  lv_obj_t *wifi_prev_btn;
  lv_obj_t *wifi_next_btn;
  lv_obj_t *wifi_page_label;
  lv_obj_t *wifi_password_overlay;
  lv_obj_t *wifi_password_panel;
  int wifi_network_page;
  int wifi_connecting_index;

  lv_obj_t *facility_form;
  lv_obj_t *facility_name_ta;
  lv_obj_t *facility_country_ta;
  lv_obj_t *facility_address_ta;
  lv_obj_t *facility_city_ta;
  lv_obj_t *facility_zip_ta;
  lv_obj_t *facility_state_ta;
  lv_obj_t *facility_timezone_ta;
  lv_obj_t *facility_type_ta;
  lv_obj_t *facility_capacity_ta;
  lv_obj_t *facility_operator_ta;
  lv_obj_t *facility_status_label;
  lv_obj_t *facility_save_btn;
  int facility_page;

  UI_Screen current_screen;
  bool wifi_connecting;
  bool wifi_connected;
  char wifi_status[128];
  char wifi_ssid[64];
  char wifi_ip[32];
  char wifi_networks[20][64];
  int wifi_network_count;
  int wifi_network_selected;
} UI;

void ui_init(UI *_UI);
void ui_show_screen(UI *_UI, UI_Screen _screen);
void ui_set_wifi_form_status(UI *_UI, const char *_msg, bool _error);
void ui_set_wifi_busy(UI *_UI, bool _busy);
void ui_tick(UI *_UI);
void ui_set_footer_text(UI *_UI, const char *_text);
void ui_set_wifi_status(UI *_UI, bool _connected, const char *_ssid,
                        const char *_ip);
void ui_set_wifi_network_list(UI *_UI, const char *_options);

#endif
