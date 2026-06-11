#ifndef __UI_H_
#define __UI_H_

#include "dashboard_types.h"
#include "facility_config.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

typedef enum {
  UI_RANGE_24H = 0,
  UI_RANGE_7D,
  UI_RANGE_30D,
} UI_Range;

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
  lv_obj_t *forecast_table_prev_btn;
  lv_obj_t *forecast_table_next_btn;
  lv_obj_t *forecast_table_page_label;
  lv_obj_t *forecast_range_24h_btn;
  lv_obj_t *forecast_range_7d_btn;
  lv_obj_t *forecast_range_30d_btn;
  lv_obj_t *forecast_x_axis_label;
  lv_obj_t *forecast_y_axis_label;
  lv_chart_series_t *forecast_temp_series;
  lv_obj_t *forecast_detail_overlay;
  lv_obj_t *forecast_detail_page1;
  lv_obj_t *forecast_detail_page2;
  lv_obj_t *forecast_detail_prev_btn;
  lv_obj_t *forecast_detail_next_btn;
  lv_obj_t *forecast_detail_page_label;
  int forecast_detail_page;
  int forecast_table_page;
  UI_Range forecast_range;
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
  lv_obj_t *keyboard;
  lv_obj_t *wifi_ssid_label;
  lv_obj_t *wifi_network_rows[5];
  lv_obj_t *wifi_network_labels[5];
  lv_obj_t *wifi_prev_btn;
  lv_obj_t *wifi_next_btn;
  lv_obj_t *wifi_page_label;
  lv_obj_t *wifi_password_overlay;
  lv_obj_t *wifi_password_panel;
  lv_obj_t *wifi_show_password_cb;
  int wifi_network_page;
  int wifi_connecting_index;

  lv_obj_t *facility_form;
  lv_obj_t *facility_name_ta;
  lv_obj_t *facility_lat_ta;
  lv_obj_t *facility_lon_ta;
  lv_obj_t *facility_energy_zone_ta;
  lv_obj_t *facility_status_label;
  lv_obj_t *facility_save_btn;
  int facility_page;

  lv_obj_t *nav_clock_label;
  lv_obj_t *nav_date_label;
  lv_obj_t *home_outdoor_icon_label;
  lv_obj_t *home_outdoor_value_label;
  lv_obj_t *home_outdoor_sub_label;
  lv_obj_t *home_price_icon_label;
  lv_obj_t *home_price_value_label;
  lv_obj_t *home_price_sub_label;
  lv_obj_t *home_meter_icon_label;
  lv_obj_t *home_meter_value_label;
  lv_obj_t *home_meter_sub_label;
  lv_obj_t *home_indoor_temp_icon_label;
  lv_obj_t *home_indoor_temp_value_label;
  lv_obj_t *home_indoor_temp_sub_label;
  lv_obj_t *home_humidity_icon_label;
  lv_obj_t *home_humidity_value_label;
  lv_obj_t *home_humidity_sub_label;
  lv_obj_t *home_pressure_icon_label;
  lv_obj_t *home_pressure_value_label;
  lv_obj_t *home_pressure_sub_label;
  uint16_t current_year;
  uint8_t current_month;
  uint8_t current_day;
  uint8_t current_hour;
  uint8_t current_minute;
  bool has_time;
  bool has_date;
  bool has_indoor_climate;
  bool has_live_power;
  float indoor_temperature_c;
  float indoor_pressure_hpa;
  float indoor_humidity_rh;
  uint32_t live_power_w;

  UI_Screen current_screen;
  bool wifi_connecting;
  bool wifi_connected;
  bool setup_wizard_active;
  bool setup_missing_wifi;
  bool setup_missing_facility;
  char wifi_status[128];
  char wifi_ssid[64];
  char wifi_ip[32];
  char wifi_networks[20][64];
  int wifi_network_count;
  int wifi_network_selected;
  Facility_Config facility_cfg;

  lv_obj_t *energy_kwh_chart;
  lv_obj_t *energy_cost_chart;
  lv_obj_t *energy_power_chart;
  lv_obj_t *energy_power_label;
  lv_obj_t *energy_max_power_label;
  lv_obj_t *energy_range_btn_row;
  lv_obj_t *energy_range_24h_btn;
  lv_obj_t *energy_range_7d_btn;
  lv_obj_t *energy_range_30d_btn;
  lv_obj_t *energy_range_notice_label;
  lv_chart_series_t *energy_kwh_series;
  lv_chart_series_t *energy_cost_series;
  lv_chart_series_t *energy_power_series;
  lv_obj_t *energy_detail_overlay;
  UI_Range energy_range;

  WeatherData cached_weather;
  ElectricityData cached_electricity;
  RealtimeData cached_realtime;
  bool has_dashboard_data;
} UI;

void ui_init(UI *_UI);
void ui_show_screen(UI *_UI, UI_Screen _screen);
void ui_set_wifi_form_status(UI *_UI, const char *_msg, bool _error);
void ui_set_wifi_busy(UI *_UI, bool _busy);
void ui_tick(UI *_UI);
void ui_set_time(UI *_UI, uint8_t h, uint8_t m, uint8_t s);
void ui_set_footer_text(UI *_UI, const char *_text);
void ui_set_wifi_status(UI *_UI, bool _connected, const char *_ssid,
                        const char *_ip);
void ui_set_wifi_network_list(UI *_UI, const char *_options);
void ui_set_date(UI *_UI, uint16_t year, uint8_t month, uint8_t day);
void ui_set_indoor_climate(UI *_UI, float temperature_c, float pressure_hpa,
                           float humidity_rh);
void ui_set_live_power(UI *_UI, uint32_t power_w);
void ui_set_dashboard_data(UI *ui, const WeatherData *w,
                           const ElectricityData *e, const RealtimeData *r);
void ui_start_setup_wizard(UI *_UI, bool missing_wifi, bool missing_facility);

#ifdef __cplusplus
}
#endif

#endif
