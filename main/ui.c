#include "ui.h"
#include "dashboard_data_api.h"
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
static void ui_setup_finish_if_complete(UI *_UI);
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
static void ui_update_forecast_data(UI *ui, const WeatherData *weather);
static int ui_forecast_table_page_count(void);
static void ui_update_wifi_rows(UI *_UI);
static void ui_open_wifi_password(UI *_UI, int _idx);
static void ui_close_wifi_password(UI *_UI);
static void ui_build_facility_page(UI *_UI);

static void nav_event_cb(lv_event_t *_event);
static void back_event_cb(lv_event_t *_event);
static void settings_card_event_cb(lv_event_t *_event);
static void view_toggle_event_cb(lv_event_t *_event);
static void forecast_range_event_cb(lv_event_t *_event);
static void energy_range_event_cb(lv_event_t *_event);
static void wifi_scan_btn_event_cb(lv_event_t *_event);
static void wifi_network_row_event_cb(lv_event_t *_event);
static void wifi_prev_page_event_cb(lv_event_t *_event);
static void wifi_next_page_event_cb(lv_event_t *_event);
static void connect_event_cb(lv_event_t *_event);
static void wifi_cancel_event_cb(lv_event_t *_event);
static void wifi_show_password_event_cb(lv_event_t *_event);
static void facility_prev_event_cb(lv_event_t *_event);
static void facility_next_event_cb(lv_event_t *_event);
static void facility_save_event_cb(lv_event_t *_event);
static void forecast_chart_event_cb(lv_event_t *_event);
static void forecast_detail_close_event_cb(lv_event_t *_event);
static void forecast_detail_page_event_cb(lv_event_t *_event);
static void forecast_table_page_event_cb(lv_event_t *_event);
static void energy_chart_event_cb(lv_event_t *_event);
static void energy_detail_close_event_cb(lv_event_t *_event);
static void energy_detail_info_event_cb(lv_event_t *_event);

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

/****************************CHART********************/

#define UI_ENERGY_TIME_TICKS_MAX 6
#define UI_FORECAST_CHART_POINTS 24
#define UI_FORECAST_TIME_TICKS 5
#define UI_FORECAST_TABLE_ROWS_PER_PAGE 4

typedef enum {
  UI_ENERGY_DETAIL_CONSUMPTION = 0,
  UI_ENERGY_DETAIL_COST,
  UI_ENERGY_DETAIL_POWER,
} UI_EnergyDetailKind;

static int ui_max_float_scaled(const float *v, int count, float scale) {
  int max_v = 1;

  for (int i = 0; i < count; i++) {
    int s = (int)(v[i] * scale);
    if (s > max_v) {
      max_v = s;
    }
  }

  return max_v;
}

static int ui_max_u32(const uint32_t *v, int count) {
  int max_v = 1;
  for (int i = 0; i < count; i++) {
    if ((int)v[i] > max_v)
      max_v = (int)v[i];
  }
  return max_v;
}

static float ui_avg_float_masked(const float *v, const bool *has_data, int count) {
  float total = 0.0f;
  int samples = 0;

  if (!v || !has_data || count <= 0)
    return 0.0f;

  for (int i = 0; i < count; i++) {
    if (!has_data[i])
      continue;
    total += v[i];
    samples++;
  }

  return samples > 0 ? total / (float)samples : 0.0f;
}

static int ui_percent_delta_float(float value, float avg) {
  if (avg <= 0.0f)
    return 0;

  return (int)(((value - avg) * 100.0f) / avg);
}

static int ui_percent_of_u32(uint32_t value, uint32_t max) {
  if (max == 0)
    return 0;

  return (int)(((uint64_t)value * 100ULL) / (uint64_t)max);
}

static void ui_format_signed_percent(char *buf, size_t size, int percent) {
  if (!buf || size == 0)
    return;

  snprintf(buf, size, "%+d%%", percent);
}

static int ui_energy_point_count(const UI *ui) {
  if (!ui)
    return 24;
  if (ui->cached_realtime.point_count > 0 &&
      ui->cached_realtime.point_count <= DASHBOARD_ENERGY_MAX_POINTS) {
    return ui->cached_realtime.point_count;
  }
  return ui->energy_range == UI_RANGE_30D ? 30
         : ui->energy_range == UI_RANGE_7D ? 7
                                           : 24;
}

static int ui_energy_tick_count(const UI *ui) {
  int point_count = ui_energy_point_count(ui);
  return point_count <= UI_ENERGY_TIME_TICKS_MAX ? point_count
                                                 : UI_ENERGY_TIME_TICKS_MAX;
}

static int ui_energy_tick_to_point_index(const UI *ui, int tick) {
  int point_count = ui_energy_point_count(ui);
  int tick_count = ui_energy_tick_count(ui);
  if (point_count <= 0 || tick_count <= 0)
    return 0;
  if (tick < 0)
    tick = 0;
  if (tick >= tick_count)
    tick = tick_count - 1;
  if (tick_count == 1)
    return 0;
  return (tick * (point_count - 1)) / (tick_count - 1);
}

static void ui_energy_chart_time_label(const UI *ui, int tick, char *buf,
                                       uint32_t buf_size) {
  if (buf == NULL || buf_size == 0)
    return;

  if (ui && ui->cached_realtime.point_count > 0) {
    int index = ui_energy_tick_to_point_index(ui, tick);
    snprintf(buf, buf_size, "%s", ui->cached_realtime.labels[index]);
    return;
  }

  if (tick == UI_ENERGY_TIME_TICKS_MAX - 1) {
    snprintf(buf, buf_size, "24:00");
  } else {
    snprintf(buf, buf_size, "%02d:00", tick * 6);
  }
}

static void ui_energy_hour_range_label(uint32_t hour, char *buf,
                                       size_t buf_size) {
  if (!buf || buf_size == 0)
    return;

  uint32_t start = hour % 24;
  uint32_t end = (start + 1) % 24;
  snprintf(buf, buf_size, "%02lu:00-%02lu:00", (unsigned long)start,
           (unsigned long)end);
}

static void ui_energy_point_label(const UI *ui, uint32_t index, char *buf,
                                  size_t buf_size) {
  if (!buf || buf_size == 0)
    return;

  if (!ui || index >= DASHBOARD_ENERGY_MAX_POINTS) {
    snprintf(buf, buf_size, "--");
    return;
  }

  if (ui->cached_realtime.interval_minutes >= 1440 &&
      ui->cached_realtime.labels[index][0] != '\0') {
    snprintf(buf, buf_size, "%s", ui->cached_realtime.labels[index]);
    return;
  }

  ui_energy_hour_range_label(index, buf, buf_size);
}

static void ui_forecast_time_axis_label(int tick, char *buf,
                                        uint32_t buf_size) {
  if (buf == NULL || buf_size == 0)
    return;

  if (tick < 0)
    tick = 0;
  if (tick >= UI_FORECAST_TIME_TICKS)
    tick = UI_FORECAST_TIME_TICKS - 1;

  if (tick == UI_FORECAST_TIME_TICKS - 1) {
    snprintf(buf, buf_size, "24");
  } else {
    snprintf(buf, buf_size, "%02d", tick * 6);
  }
}

static void ui_forecast_temp_axis_label(int scaled_temp, char *buf,
                                        uint32_t buf_size) {
  if (buf == NULL || buf_size == 0)
    return;

  snprintf(buf, buf_size, "%dC", scaled_temp / 10);
}

static const char *ui_weather_code_summary(uint16_t code) {
  if (code == 0)
    return "Clear";
  if (code == 1 || code == 2)
    return "Partly cloudy";
  if (code == 3)
    return "Cloudy";
  if (code == 45 || code == 48)
    return "Fog";
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82))
    return "Rain";
  if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86))
    return "Snow";
  if (code >= 95)
    return "Thunder";
  return "Forecast";
}

static void chart_time_axis_draw_cb(lv_event_t *e) {
  UI *ui = lv_event_get_user_data(e);
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
  if (!lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                   LV_CHART_DRAW_PART_TICK_LABEL)) {
    return;
  }

  if (dsc->id != LV_CHART_AXIS_PRIMARY_X || dsc->text == NULL)
    return;

  ui_energy_chart_time_label(ui, dsc->value, dsc->text, dsc->text_length);
}

static void forecast_axis_draw_cb(lv_event_t *e) {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
  if (!lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                   LV_CHART_DRAW_PART_TICK_LABEL)) {
    return;
  }

  if (dsc->text == NULL)
    return;

  if (dsc->id == LV_CHART_AXIS_PRIMARY_X) {
    ui_forecast_time_axis_label(dsc->value, dsc->text, dsc->text_length);
  } else if (dsc->id == LV_CHART_AXIS_PRIMARY_Y) {
    ui_forecast_temp_axis_label(dsc->value, dsc->text, dsc->text_length);
  }
}

static void ui_style_chart(lv_obj_t *chart) {
  lv_obj_set_style_bg_color(chart, lv_color_hex(C_CARD_DARK), 0);
  lv_obj_set_style_border_color(chart, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_border_width(chart, 1, 0);
  lv_obj_set_style_radius(chart, 8, 0);
  lv_obj_clear_flag(chart, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(chart, LV_SCROLLBAR_MODE_OFF);
  lv_chart_set_div_line_count(chart, 5, 6);
}

static void ui_configure_energy_chart(UI *ui, lv_obj_t *chart) {
  ui_style_chart(chart);
  lv_chart_set_point_count(chart, ui_energy_point_count(ui));
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 4, 2,
                         ui_energy_tick_count(ui), 1, true, 42);
  lv_obj_add_event_cb(chart, chart_time_axis_draw_cb, LV_EVENT_DRAW_PART_BEGIN,
                      ui);
}

static lv_obj_t *ui_create_range_button(lv_obj_t *parent, const char *text,
                                        UI_Range range, UI *ui,
                                        lv_event_cb_t cb) {
  lv_obj_t *btn = ui_create_button(parent, text, lv_color_hex(C_CARD));
  lv_obj_set_size(btn, 56, 32);
  lv_obj_set_user_data(btn, (void *)(uintptr_t)range);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, ui);
  return btn;
}

static void ui_set_range_btn_state(lv_obj_t *btn, bool active) {
  if (!btn)
    return;

  lv_obj_set_style_bg_color(btn, lv_color_hex(active ? C_BLUE : C_CARD), 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(active ? C_BLUE : C_BORDER),
                                0);
}

static void ui_update_forecast_range_buttons(UI *ui) {
  if (!ui)
    return;

  ui_set_range_btn_state(ui->forecast_range_24h_btn,
                         ui->forecast_range == UI_RANGE_24H);
  ui_set_range_btn_state(ui->forecast_range_7d_btn,
                         ui->forecast_range == UI_RANGE_7D);
  ui_set_range_btn_state(ui->forecast_range_30d_btn,
                         ui->forecast_range == UI_RANGE_30D);
}

static void ui_apply_forecast_view(UI *ui) {
  if (!ui)
    return;

  bool multi_day = ui->forecast_range != UI_RANGE_24H;
  UI_ViewMode effective_view =
      multi_day ? UI_VIEW_TABLE : ui->forecast_view_mode;
  bool show_table = effective_view == UI_VIEW_TABLE;

  if (ui->forecast_chart) {
    if (show_table || multi_day)
      lv_obj_add_flag(ui->forecast_chart, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_clear_flag(ui->forecast_chart, LV_OBJ_FLAG_HIDDEN);
  }

  if (ui->forecast_x_axis_label) {
    if (show_table || multi_day)
      lv_obj_add_flag(ui->forecast_x_axis_label, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_clear_flag(ui->forecast_x_axis_label, LV_OBJ_FLAG_HIDDEN);
  }

  if (ui->forecast_y_axis_label) {
    if (show_table || multi_day)
      lv_obj_add_flag(ui->forecast_y_axis_label, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_clear_flag(ui->forecast_y_axis_label, LV_OBJ_FLAG_HIDDEN);
  }

  if (ui->forecast_table) {
    if (show_table)
      lv_obj_clear_flag(ui->forecast_table, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(ui->forecast_table, LV_OBJ_FLAG_HIDDEN);
  }

  if (ui->forecast_table_prev_btn) {
    if (show_table && !multi_day && ui->forecast_table_page > 0)
      lv_obj_clear_flag(ui->forecast_table_prev_btn, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(ui->forecast_table_prev_btn, LV_OBJ_FLAG_HIDDEN);
  }

  if (ui->forecast_table_next_btn) {
    if (show_table && !multi_day &&
        ui->forecast_table_page < ui_forecast_table_page_count() - 1)
      lv_obj_clear_flag(ui->forecast_table_next_btn, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(ui->forecast_table_next_btn, LV_OBJ_FLAG_HIDDEN);
  }

  if (ui->forecast_table_page_label) {
    if (show_table && !multi_day)
      lv_obj_clear_flag(ui->forecast_table_page_label, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(ui->forecast_table_page_label, LV_OBJ_FLAG_HIDDEN);
  }

  ui_set_range_btn_state(ui->forecast_graph_btn, !show_table);
  ui_set_range_btn_state(ui->forecast_table_btn, show_table);
}

static void ui_update_energy_range_buttons(UI *ui) {
  if (!ui)
    return;

  ui_set_range_btn_state(ui->energy_range_24h_btn,
                         ui->energy_range == UI_RANGE_24H);
  ui_set_range_btn_state(ui->energy_range_7d_btn,
                         ui->energy_range == UI_RANGE_7D);
  ui_set_range_btn_state(ui->energy_range_30d_btn,
                         ui->energy_range == UI_RANGE_30D);
}

static void ui_set_bar_chart_float(lv_obj_t *chart, lv_chart_series_t *ser,
                                   const float *values, int count,
                                   float scale) {
  if (!chart || !ser || !values)
    return;

  lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
  lv_chart_set_point_count(chart, count);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0,
                     ui_max_float_scaled(values, count, scale) + 5);

  for (int i = 0; i < count; i++) {
    lv_chart_set_value_by_id(chart, ser, i, (lv_coord_t)(values[i] * scale));
  }

  lv_chart_refresh(chart);
}

static lv_obj_t *ui_energy_detail_row(lv_obj_t *parent, const char *name,
                                      const char *value) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, LV_PCT(100), 30);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *name_label = ui_create_label(row, name, lv_color_hex(C_MUTED));
  lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *value_label = ui_create_label(row, value, lv_color_white());
  lv_obj_align(value_label, LV_ALIGN_RIGHT_MID, 0, 0);

  return row;
}

static void ui_close_energy_detail(UI *ui) {
  if (!ui || !ui->energy_detail_overlay)
    return;

  lv_obj_del(ui->energy_detail_overlay);
  ui->energy_detail_overlay = NULL;
}

static void ui_close_forecast_detail(UI *ui) {
  if (!ui || !ui->forecast_detail_overlay)
    return;

  lv_obj_del(ui->forecast_detail_overlay);
  ui->forecast_detail_overlay = NULL;
  ui->forecast_detail_page1 = NULL;
  ui->forecast_detail_page2 = NULL;
  ui->forecast_detail_prev_btn = NULL;
  ui->forecast_detail_next_btn = NULL;
  ui->forecast_detail_page_label = NULL;
  ui->forecast_detail_page = 0;
}

static void ui_set_forecast_detail_page(UI *ui, int page) {
  if (!ui || !ui->forecast_detail_page1 || !ui->forecast_detail_page2)
    return;

  if (page < 0)
    page = 0;
  if (page > 1)
    page = 1;

  ui->forecast_detail_page = page;

  if (page == 0) {
    lv_obj_clear_flag(ui->forecast_detail_page1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->forecast_detail_page2, LV_OBJ_FLAG_HIDDEN);
    if (ui->forecast_detail_prev_btn)
      lv_obj_add_state(ui->forecast_detail_prev_btn, LV_STATE_DISABLED);
    if (ui->forecast_detail_next_btn)
      lv_obj_clear_state(ui->forecast_detail_next_btn, LV_STATE_DISABLED);
  } else {
    lv_obj_add_flag(ui->forecast_detail_page1, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui->forecast_detail_page2, LV_OBJ_FLAG_HIDDEN);
    if (ui->forecast_detail_prev_btn)
      lv_obj_clear_state(ui->forecast_detail_prev_btn, LV_STATE_DISABLED);
    if (ui->forecast_detail_next_btn)
      lv_obj_add_state(ui->forecast_detail_next_btn, LV_STATE_DISABLED);
  }

  if (ui->forecast_detail_page_label) {
    lv_label_set_text_fmt(ui->forecast_detail_page_label, "%d / 2", page + 1);
  }
}

static void ui_show_forecast_detail(UI *ui, uint32_t hour) {
  if (!ui || !ui->has_dashboard_data || hour >= UI_FORECAST_CHART_POINTS ||
      !ui->cached_weather.valid)
    return;

  ui_close_forecast_detail(ui);

  const WeatherData *weather = &ui->cached_weather;
  char hour_label[24];
  char primary[32];
  char temp_value[32];
  char rain_value[32];
  char wind_value[32];
  char solar_value[32];
  char condition_value[32];
  char code_value[32];
  char rain_detail[40];

  ui_energy_hour_range_label(hour, hour_label, sizeof(hour_label));
  snprintf(primary, sizeof(primary), "%.1fC", weather->temp_c_24h[hour]);
  snprintf(temp_value, sizeof(temp_value), "%.1fC", weather->temp_c_24h[hour]);
  snprintf(rain_value, sizeof(rain_value), "%u%%",
           weather->rain_percent_24h[hour]);
  snprintf(wind_value, sizeof(wind_value), "%.0f km/h",
           weather->wind_kmh_24h[hour]);
  snprintf(solar_value, sizeof(solar_value), "%u W/m2",
           weather->shortwave_wm2_24h[hour]);
  snprintf(condition_value, sizeof(condition_value), "%s",
           ui_weather_code_summary(weather->weather_code_24h[hour]));
  snprintf(code_value, sizeof(code_value), "%u",
           weather->weather_code_24h[hour]);
  snprintf(rain_detail, sizeof(rain_detail), "Probability %u%%",
           weather->rain_percent_24h[hour]);

  ui->forecast_detail_overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(ui->forecast_detail_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(ui->forecast_detail_overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(ui->forecast_detail_overlay, LV_OPA_70, 0);
  lv_obj_set_style_border_width(ui->forecast_detail_overlay, 0, 0);
  lv_obj_clear_flag(ui->forecast_detail_overlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *panel = lv_obj_create(ui->forecast_detail_overlay);
  lv_obj_set_size(panel, 430, 315);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, lv_color_hex(C_PANEL), 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(C_BLUE), 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_radius(panel, 8, 0);
  lv_obj_set_style_pad_all(panel, 18, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = ui_create_label(panel, hour_label, lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t *subtitle =
      ui_create_label(panel, "Weather detail", lv_color_hex(C_MUTED));
  lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 0, 24);

  lv_obj_t *close =
      ui_create_button(panel, LV_SYMBOL_CLOSE, lv_color_hex(C_CARD));
  lv_obj_set_size(close, 36, 36);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_add_event_cb(close, forecast_detail_close_event_cb, LV_EVENT_CLICKED,
                      ui);

  lv_obj_t *primary_label =
      ui_create_label(panel, primary, lv_color_hex(C_BLUE));
  lv_obj_align(primary_label, LV_ALIGN_TOP_MID, 0, 58);

  ui->forecast_detail_page1 = lv_obj_create(panel);
  lv_obj_set_size(ui->forecast_detail_page1, LV_PCT(100), 122);
  lv_obj_align(ui->forecast_detail_page1, LV_ALIGN_TOP_LEFT, 0, 98);
  lv_obj_set_style_bg_opa(ui->forecast_detail_page1, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ui->forecast_detail_page1, 0, 0);
  lv_obj_set_style_pad_all(ui->forecast_detail_page1, 0, 0);
  lv_obj_clear_flag(ui->forecast_detail_page1, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *row = ui_energy_detail_row(ui->forecast_detail_page1, "Condition",
                                       condition_value);
  lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, 0);
  row = ui_energy_detail_row(ui->forecast_detail_page1, "Temperature",
                             temp_value);
  lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, 34);
  row = ui_energy_detail_row(ui->forecast_detail_page1, "Precipitation",
                             rain_value);
  lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, 68);

  ui->forecast_detail_page2 = lv_obj_create(panel);
  lv_obj_set_size(ui->forecast_detail_page2, LV_PCT(100), 122);
  lv_obj_align(ui->forecast_detail_page2, LV_ALIGN_TOP_LEFT, 0, 98);
  lv_obj_set_style_bg_opa(ui->forecast_detail_page2, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ui->forecast_detail_page2, 0, 0);
  lv_obj_set_style_pad_all(ui->forecast_detail_page2, 0, 0);
  lv_obj_clear_flag(ui->forecast_detail_page2, LV_OBJ_FLAG_SCROLLABLE);

  row = ui_energy_detail_row(ui->forecast_detail_page2, "Wind", wind_value);
  lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, 0);
  row = ui_energy_detail_row(ui->forecast_detail_page2, "Solar", solar_value);
  lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, 34);
  row = ui_energy_detail_row(ui->forecast_detail_page2, "Weather code",
                             code_value);
  lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, 68);
  row = ui_energy_detail_row(ui->forecast_detail_page2, "Rain detail",
                             rain_detail);
  lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, 102);

  ui->forecast_detail_prev_btn =
      ui_create_button(panel, LV_SYMBOL_LEFT, lv_color_hex(C_CARD));
  lv_obj_set_size(ui->forecast_detail_prev_btn, 42, 36);
  lv_obj_align(ui->forecast_detail_prev_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_event_cb(ui->forecast_detail_prev_btn,
                      forecast_detail_page_event_cb, LV_EVENT_CLICKED, ui);
  lv_obj_set_user_data(ui->forecast_detail_prev_btn, (void *)(uintptr_t)0);

  ui->forecast_detail_next_btn =
      ui_create_button(panel, LV_SYMBOL_RIGHT, lv_color_hex(C_CARD));
  lv_obj_set_size(ui->forecast_detail_next_btn, 42, 36);
  lv_obj_align(ui->forecast_detail_next_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(ui->forecast_detail_next_btn,
                      forecast_detail_page_event_cb, LV_EVENT_CLICKED, ui);
  lv_obj_set_user_data(ui->forecast_detail_next_btn, (void *)(uintptr_t)1);

  ui->forecast_detail_page_label =
      ui_create_label(panel, "1 / 2", lv_color_hex(C_MUTED));
  lv_obj_align(ui->forecast_detail_page_label, LV_ALIGN_BOTTOM_MID, 0, -8);

  ui_set_forecast_detail_page(ui, 0);
}

static void ui_show_energy_detail(UI *ui, UI_EnergyDetailKind kind,
                                  uint32_t hour) {
  int point_count = ui_energy_point_count(ui);
  if (!ui || !ui->has_dashboard_data || hour >= (uint32_t)point_count)
    return;
  if (!ui->cached_realtime.has_data[hour])
    return;

  ui_close_energy_detail(ui);

  const RealtimeData *rt = &ui->cached_realtime;
  const ElectricityData *el = &ui->cached_electricity;
  float kwh = rt->kwh_24h[hour];
  float cost = rt->cost_24h[hour];
  uint32_t power = rt->power_24h[hour];
  float price = 0.0f;

  if (kwh > 0.001f) {
    price = cost / kwh;
  } else if (el->valid) {
    price = el->sek_24h[hour];
  }

  const char *detail_title = "Consumption detail";
  lv_color_t accent = lv_color_hex(C_GREEN);
  char primary[32];
  char comparison_label[24];
  char comparison_value[24];
  char info_text[220];

  if (kind == UI_ENERGY_DETAIL_COST) {
    detail_title = "Cost detail";
    accent = lv_color_hex(C_YELLOW);
    snprintf(primary, sizeof(primary), "%.2f SEK", cost);
    snprintf(comparison_label, sizeof(comparison_label), "Vs avg cost");
    ui_format_signed_percent(
        comparison_value, sizeof(comparison_value),
        ui_percent_delta_float(
            cost, ui_avg_float_masked(rt->cost_24h, rt->has_data, point_count)));
    snprintf(info_text, sizeof(info_text),
             "Cost is the estimated SEK spent in this period. Price is SEK "
             "per kWh. Consumption shows the energy that created the cost.");
  } else if (kind == UI_ENERGY_DETAIL_POWER) {
    detail_title = "Power detail";
    accent = lv_color_hex(C_BLUE);
    snprintf(primary, sizeof(primary), "%lu W", (unsigned long)power);
    if (ui->energy_range == UI_RANGE_7D) {
      snprintf(comparison_label, sizeof(comparison_label), "Vs max 7d");
    } else if (ui->energy_range == UI_RANGE_30D) {
      snprintf(comparison_label, sizeof(comparison_label), "Vs max 30d");
    } else {
      snprintf(comparison_label, sizeof(comparison_label), "Vs max 24h");
    }
    snprintf(comparison_value, sizeof(comparison_value), "%d%%",
             ui_percent_of_u32(power, rt->max_power_w_24h));
    snprintf(info_text, sizeof(info_text),
             "Power is the momentary load measured for this hour. Consumption "
             "is accumulated energy. Cost combines consumption with price.");
  } else {
    snprintf(primary, sizeof(primary), "%.2f kWh", kwh);
    snprintf(comparison_label, sizeof(comparison_label), "Vs avg use");
    ui_format_signed_percent(
        comparison_value, sizeof(comparison_value),
        ui_percent_delta_float(
            kwh, ui_avg_float_masked(rt->kwh_24h, rt->has_data, point_count)));
    snprintf(info_text, sizeof(info_text),
             "Consumption is the energy used during this period. Cost is the "
             "estimated SEK for that energy. Power shows load in watts.");
  }

  char hour_label[24];
  char kwh_value[32];
  char cost_value[32];
  char price_value[32];
  char power_value[32];
  ui_energy_point_label(ui, hour, hour_label, sizeof(hour_label));
  snprintf(kwh_value, sizeof(kwh_value), "%.2f kWh", kwh);
  snprintf(cost_value, sizeof(cost_value), "%.2f SEK", cost);
  snprintf(price_value, sizeof(price_value), "%.2f SEK/kWh", price);
  snprintf(power_value, sizeof(power_value), "%lu W", (unsigned long)power);

  ui->energy_detail_overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(ui->energy_detail_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(ui->energy_detail_overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(ui->energy_detail_overlay, LV_OPA_70, 0);
  lv_obj_set_style_border_width(ui->energy_detail_overlay, 0, 0);
  lv_obj_clear_flag(ui->energy_detail_overlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *panel = lv_obj_create(ui->energy_detail_overlay);
  lv_obj_set_size(panel, 430, 330);
  lv_obj_center(panel);
  lv_obj_set_style_bg_color(panel, lv_color_hex(C_PANEL), 0);
  lv_obj_set_style_border_color(panel, accent, 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_radius(panel, 8, 0);
  lv_obj_set_style_pad_all(panel, 18, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = ui_create_label(panel, hour_label, lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t *subtitle =
      ui_create_label(panel, detail_title, lv_color_hex(C_MUTED));
  lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 0, 24);

  lv_obj_t *close =
      ui_create_button(panel, LV_SYMBOL_CLOSE, lv_color_hex(C_CARD));
  lv_obj_set_size(close, 36, 36);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_add_event_cb(close, energy_detail_close_event_cb, LV_EVENT_CLICKED,
                      ui);

  lv_obj_t *info = ui_create_button(panel, "i", lv_color_hex(C_CARD));
  lv_obj_set_size(info, 36, 36);
  lv_obj_align(info, LV_ALIGN_TOP_RIGHT, -44, 0);

  lv_obj_t *primary_label = ui_create_label(panel, primary, accent);
  lv_obj_set_style_text_font(primary_label, &notosans_14, 0);
  lv_obj_align(primary_label, LV_ALIGN_TOP_MID, 0, 58);

  int y = 102;
  if (kind != UI_ENERGY_DETAIL_CONSUMPTION) {
    lv_obj_t *row = ui_energy_detail_row(panel, "Consumption", kwh_value);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, y);
    y += 34;
  }
  if (kind != UI_ENERGY_DETAIL_COST) {
    lv_obj_t *row = ui_energy_detail_row(panel, "Cost", cost_value);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, y);
    y += 34;
  }
  lv_obj_t *price_row = ui_energy_detail_row(panel, "Price", price_value);
  lv_obj_align(price_row, LV_ALIGN_TOP_LEFT, 0, y);
  y += 34;

  if (kind != UI_ENERGY_DETAIL_POWER) {
    lv_obj_t *row = ui_energy_detail_row(panel, "Power", power_value);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, y);
    y += 34;
  }

  lv_obj_t *comparison_row =
      ui_energy_detail_row(panel, comparison_label, comparison_value);
  lv_obj_align(comparison_row, LV_ALIGN_TOP_LEFT, 0, y);

  lv_obj_t *info_box = lv_obj_create(panel);
  lv_obj_set_size(info_box, LV_PCT(100), 70);
  lv_obj_align(info_box, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_color(info_box, lv_color_hex(C_CARD_DARK), 0);
  lv_obj_set_style_border_color(info_box, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_border_width(info_box, 1, 0);
  lv_obj_set_style_radius(info_box, 8, 0);
  lv_obj_set_style_pad_all(info_box, 10, 0);
  lv_obj_clear_flag(info_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(info_box, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *info_label =
      ui_create_label(info_box, info_text, lv_color_hex(C_MUTED));
  lv_obj_set_width(info_label, LV_PCT(100));
  lv_label_set_long_mode(info_label, LV_LABEL_LONG_WRAP);
  lv_obj_align(info_label, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_set_user_data(info, info_box);
  lv_obj_add_event_cb(info, energy_detail_info_event_cb, LV_EVENT_CLICKED, ui);
}

static void ui_set_line_chart_u32(lv_obj_t *chart, lv_chart_series_t *ser,
                                  const uint32_t *values, int count) {
  if (!chart || !ser || !values)
    return;

  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(chart, count);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0,
                     ui_max_u32(values, count) + 200);

  for (int i = 0; i < count; i++) {
    lv_chart_set_value_by_id(chart, ser, i, (lv_coord_t)values[i]);
  }

  lv_chart_refresh(chart);
}

static void ui_update_energy_range_view(UI *ui) {
  if (!ui)
    return;

  ui_update_energy_range_buttons(ui);
  lv_obj_t *objs[] = {ui->energy_kwh_chart, ui->energy_cost_chart,
                      ui->energy_power_chart, ui->energy_power_label,
                      ui->energy_max_power_label};

  for (int i = 0; i < 5; i++) {
    if (!objs[i])
      continue;
    lv_obj_clear_flag(objs[i], LV_OBJ_FLAG_HIDDEN);
  }

  if (ui->energy_range_notice_label) {
    const char *caption = "Showing 24h profile";
    if (ui->energy_range == UI_RANGE_7D) {
      caption = "Showing 7-day historical profile";
    } else if (ui->energy_range == UI_RANGE_30D) {
      caption = "Showing 30-day historical profile";
    }
    lv_label_set_text(ui->energy_range_notice_label, caption);
    lv_obj_clear_flag(ui->energy_range_notice_label, LV_OBJ_FLAG_HIDDEN);
  }
}

void ui_set_dashboard_data(UI *ui, const WeatherData *weather,
                           const ElectricityData *electricity,
                           const RealtimeData *realtime) {
  if (ui == NULL || weather == NULL || electricity == NULL ||
      realtime == NULL) {
    return;
  }

  ui->cached_weather = *weather;
  ui->cached_electricity = *electricity;
  ui->cached_realtime = *realtime;
  ui->has_dashboard_data = true;

  if (weather->valid) {
    ui_update_forecast_data(ui, weather);
  }

  if (realtime->valid == false) {
    return;
  }

  ui_update_energy_range_view(ui);

  int point_count = ui_energy_point_count(ui);
  int tick_count = ui_energy_tick_count(ui);
  if (point_count <= 0)
    return;

  if (ui->energy_kwh_chart != NULL && ui->energy_kwh_series != NULL) {
    lv_chart_set_point_count(ui->energy_kwh_chart, point_count);
    lv_chart_set_axis_tick(ui->energy_kwh_chart, LV_CHART_AXIS_PRIMARY_X, 4, 2,
                           tick_count, 1, true, 42);
    ui_set_bar_chart_float(ui->energy_kwh_chart, ui->energy_kwh_series,
                           realtime->kwh_24h, point_count, 100.0f);
  }

  if (ui->energy_cost_chart != NULL && ui->energy_cost_series != NULL) {
    lv_chart_set_point_count(ui->energy_cost_chart, point_count);
    lv_chart_set_axis_tick(ui->energy_cost_chart, LV_CHART_AXIS_PRIMARY_X, 4, 2,
                           tick_count, 1, true, 42);
    ui_set_bar_chart_float(ui->energy_cost_chart, ui->energy_cost_series,
                           realtime->cost_24h, point_count, 100.0f);
  }

  if (ui->energy_power_chart != NULL && ui->energy_power_series != NULL) {
    lv_chart_set_point_count(ui->energy_power_chart, point_count);
    lv_chart_set_axis_tick(ui->energy_power_chart, LV_CHART_AXIS_PRIMARY_X, 4, 2,
                           tick_count, 1, true, 42);
    ui_set_line_chart_u32(ui->energy_power_chart, ui->energy_power_series,
                          realtime->power_24h, point_count);
  }

  if (ui->energy_power_label != NULL) {
    if (ui->energy_range == UI_RANGE_24H) {
      lv_label_set_text_fmt(ui->energy_power_label, "Now: %lu W",
                            (unsigned long)realtime->power_w);
    } else {
      lv_label_set_text_fmt(ui->energy_power_label, "Latest avg: %lu W",
                            (unsigned long)realtime->power_w);
    }
  }

  if (ui->energy_max_power_label != NULL) {
    if (ui->energy_range == UI_RANGE_7D) {
      lv_label_set_text_fmt(ui->energy_max_power_label, "Max 7d: %lu W",
                            (unsigned long)realtime->max_power_w_24h);
    } else if (ui->energy_range == UI_RANGE_30D) {
      lv_label_set_text_fmt(ui->energy_max_power_label, "Max 30d: %lu W",
                            (unsigned long)realtime->max_power_w_24h);
    } else {
      lv_label_set_text_fmt(ui->energy_max_power_label, "Max 24h: %lu W",
                            (unsigned long)realtime->max_power_w_24h);
    }
  }
}
/*--------------------------------------------------------------*/
void ui_init(UI *_UI) {
  if (!_UI)
    return;
  memset(_UI, 0, sizeof(UI));
  _UI->wifi_connected = false;
  _UI->wifi_connecting_index = -1;
  _UI->forecast_view_mode = UI_VIEW_GRAPH;
  _UI->elpriser_view_mode = UI_VIEW_GRAPH;
  _UI->forecast_range = UI_RANGE_24H;
  _UI->energy_range = UI_RANGE_24H;
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
  ui_close_energy_detail(_UI);
  ui_close_forecast_detail(_UI);
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
  _UI->forecast_table_prev_btn = NULL;
  _UI->forecast_table_next_btn = NULL;
  _UI->forecast_table_page_label = NULL;
  _UI->forecast_range_24h_btn = NULL;
  _UI->forecast_range_7d_btn = NULL;
  _UI->forecast_range_30d_btn = NULL;
  _UI->forecast_x_axis_label = NULL;
  _UI->forecast_y_axis_label = NULL;
  _UI->forecast_temp_series = NULL;
  _UI->forecast_detail_overlay = NULL;
  _UI->forecast_detail_page1 = NULL;
  _UI->forecast_detail_page2 = NULL;
  _UI->forecast_detail_prev_btn = NULL;
  _UI->forecast_detail_next_btn = NULL;
  _UI->forecast_detail_page_label = NULL;
  _UI->forecast_detail_page = 0;
  _UI->forecast_table_page = 0;
  _UI->elpriser_chart = NULL;
  _UI->elpriser_table = NULL;
  _UI->elpriser_graph_btn = NULL;
  _UI->elpriser_table_btn = NULL;
  _UI->energy_kwh_chart = NULL;
  _UI->energy_cost_chart = NULL;
  _UI->energy_power_chart = NULL;
  _UI->energy_power_label = NULL;
  _UI->energy_max_power_label = NULL;
  _UI->energy_range_24h_btn = NULL;
  _UI->energy_range_7d_btn = NULL;
  _UI->energy_range_30d_btn = NULL;
  _UI->energy_range_btn_row = NULL;
  _UI->energy_range_notice_label = NULL;
  _UI->energy_kwh_series = NULL;
  _UI->energy_cost_series = NULL;
  _UI->energy_power_series = NULL;
  _UI->energy_detail_overlay = NULL;
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
  _UI->wifi_show_password_cb = NULL;
  for (int i = 0; i < 5; i++) {
    _UI->wifi_network_rows[i] = NULL;
    _UI->wifi_network_labels[i] = NULL;
  }
  _UI->facility_form = NULL;
  _UI->facility_name_ta = NULL;
  _UI->facility_status_label = NULL;
  _UI->facility_save_btn = NULL;
  _UI->facility_lat_ta = NULL;
  _UI->facility_lon_ta = NULL;
  _UI->facility_energy_zone_ta = NULL;
}

static bool ui_facility_config_complete(const Facility_Config *cfg) {
  if (!cfg)
    return false;

  return cfg->facility_name[0] != '\0' && cfg->lat[0] != '\0' &&
         cfg->lon[0] != '\0' && cfg->energy_zone >= 1 && cfg->energy_zone <= 4;
}

static void ui_setup_finish_if_complete(UI *_UI) {
  if (!_UI || !_UI->setup_wizard_active)
    return;

  if (_UI->setup_missing_wifi && !_UI->wifi_connected)
    return;

  if (_UI->setup_missing_facility &&
      !ui_facility_config_complete(&_UI->facility_cfg))
    return;

  _UI->setup_wizard_active = false;
  _UI->setup_missing_wifi = false;
  _UI->setup_missing_facility = false;
  ui_set_footer_text(_UI, "Setup complete");
  ui_show_screen(_UI, UI_SCREEN_HOME);
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

  if ((_screen == UI_SCREEN_FORECAST || _screen == UI_SCREEN_ELPRISER) &&
      _UI->has_dashboard_data) {
    ui_set_dashboard_data(_UI, &_UI->cached_weather, &_UI->cached_electricity,
                          &_UI->cached_realtime);
  }
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
  ui_create_metric_card(col3, "Current price", "0.68 SEK/kWh",
                        "Normal price level", lv_color_hex(C_YELLOW));
  ui_create_metric_card(col3, "Meter", "Waiting", "P1/HAN data not wired",
                        lv_color_hex(C_PURPLE));
}

static int ui_forecast_temp_min_scaled(const float *values) {
  int min_v = (int)(values[0] * 10.0f);

  for (int i = 1; i < UI_FORECAST_CHART_POINTS; i++) {
    int v = (int)(values[i] * 10.0f);
    if (v < min_v)
      min_v = v;
  }

  return min_v;
}

static int ui_forecast_temp_max_scaled(const float *values) {
  int max_v = (int)(values[0] * 10.0f);

  for (int i = 1; i < UI_FORECAST_CHART_POINTS; i++) {
    int v = (int)(values[i] * 10.0f);
    if (v > max_v)
      max_v = v;
  }

  return max_v;
}

static int ui_forecast_table_page_count(void) {
  return (UI_FORECAST_CHART_POINTS + UI_FORECAST_TABLE_ROWS_PER_PAGE - 1) /
         UI_FORECAST_TABLE_ROWS_PER_PAGE;
}

static void ui_update_forecast_table(UI *ui, const WeatherData *weather) {
  if (!ui || !weather || !weather->valid || !ui->forecast_table)
    return;

  int page_count = ui_forecast_table_page_count();
  if (ui->forecast_table_page < 0)
    ui->forecast_table_page = 0;
  if (ui->forecast_table_page >= page_count)
    ui->forecast_table_page = page_count - 1;

  char temp[16];
  char rain[16];
  char wind[16];
  char solar[16];

  lv_table_set_col_cnt(ui->forecast_table, 5);
  lv_table_set_row_cnt(ui->forecast_table, UI_FORECAST_TABLE_ROWS_PER_PAGE + 1);
  lv_table_set_cell_value(ui->forecast_table, 0, 0, "Time");
  lv_table_set_cell_value(ui->forecast_table, 0, 1, "Temp");
  lv_table_set_cell_value(ui->forecast_table, 0, 2, "Rain");
  lv_table_set_cell_value(ui->forecast_table, 0, 3, "Wind");
  lv_table_set_cell_value(ui->forecast_table, 0, 4, "Solar");

  int start_hour = ui->forecast_table_page * UI_FORECAST_TABLE_ROWS_PER_PAGE;
  for (int row = 0; row < UI_FORECAST_TABLE_ROWS_PER_PAGE; row++) {
    int hour = start_hour + row;
    if (hour >= UI_FORECAST_CHART_POINTS) {
      lv_table_set_cell_value(ui->forecast_table, row + 1, 0, "");
      lv_table_set_cell_value(ui->forecast_table, row + 1, 1, "");
      lv_table_set_cell_value(ui->forecast_table, row + 1, 2, "");
      lv_table_set_cell_value(ui->forecast_table, row + 1, 3, "");
      lv_table_set_cell_value(ui->forecast_table, row + 1, 4, "");
      continue;
    }

    snprintf(temp, sizeof(temp), "%.1fC", weather->temp_c_24h[hour]);
    snprintf(rain, sizeof(rain), "%u%%", weather->rain_percent_24h[hour]);
    snprintf(wind, sizeof(wind), "%.0f", weather->wind_kmh_24h[hour]);
    snprintf(solar, sizeof(solar), "%u", weather->shortwave_wm2_24h[hour]);

    lv_table_set_cell_value(ui->forecast_table, row + 1, 0,
                            weather->time_24h[hour]);
    lv_table_set_cell_value(ui->forecast_table, row + 1, 1, temp);
    lv_table_set_cell_value(ui->forecast_table, row + 1, 2, rain);
    lv_table_set_cell_value(ui->forecast_table, row + 1, 3, wind);
    lv_table_set_cell_value(ui->forecast_table, row + 1, 4, solar);
  }

  if (ui->forecast_table_page_label) {
    lv_label_set_text_fmt(ui->forecast_table_page_label, "%d / %d",
                          ui->forecast_table_page + 1, page_count);
  }

  if (ui->forecast_table_prev_btn) {
    if (ui->forecast_table_page == 0)
      lv_obj_add_state(ui->forecast_table_prev_btn, LV_STATE_DISABLED);
    else
      lv_obj_clear_state(ui->forecast_table_prev_btn, LV_STATE_DISABLED);
  }

  if (ui->forecast_table_next_btn) {
    if (ui->forecast_table_page >= page_count - 1)
      lv_obj_add_state(ui->forecast_table_next_btn, LV_STATE_DISABLED);
    else
      lv_obj_clear_state(ui->forecast_table_next_btn, LV_STATE_DISABLED);
  }

  ui_apply_forecast_view(ui);
}

static void ui_update_forecast_data(UI *ui, const WeatherData *weather) {
  if (!ui || !weather || !weather->valid)
    return;

  ui_update_forecast_range_buttons(ui);

  if (ui->forecast_range != UI_RANGE_24H) {
    if (ui->forecast_chart && ui->forecast_temp_series) {
      lv_chart_set_range(ui->forecast_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1);
      for (int i = 0; i < UI_FORECAST_CHART_POINTS; i++) {
        lv_chart_set_value_by_id(ui->forecast_chart, ui->forecast_temp_series,
                                 i, 0);
      }
      lv_chart_refresh(ui->forecast_chart);
    }

    if (ui->forecast_table) {
      lv_table_set_col_cnt(ui->forecast_table, 1);
      lv_table_set_row_cnt(ui->forecast_table, 2);
      lv_table_set_cell_value(ui->forecast_table, 0, 0,
                              ui->forecast_range == UI_RANGE_7D ? "7 days"
                                                                : "30 days");
      lv_table_set_cell_value(ui->forecast_table, 1, 0,
                              "Multi-day weather data unavailable");
    }
    ui_apply_forecast_view(ui);
    return;
  }

  if (ui->forecast_chart && ui->forecast_temp_series) {
    int min_v = ui_forecast_temp_min_scaled(weather->temp_c_24h) - 20;
    int max_v = ui_forecast_temp_max_scaled(weather->temp_c_24h) + 20;

    if (min_v == max_v)
      max_v = min_v + 10;

    lv_chart_set_range(ui->forecast_chart, LV_CHART_AXIS_PRIMARY_Y, min_v,
                       max_v);
    for (int i = 0; i < UI_FORECAST_CHART_POINTS; i++) {
      lv_chart_set_value_by_id(ui->forecast_chart, ui->forecast_temp_series, i,
                               (lv_coord_t)(weather->temp_c_24h[i] * 10.0f));
    }
    lv_chart_refresh(ui->forecast_chart);
  }

  ui_update_forecast_table(ui, weather);
}

static void ui_build_screen_forecast(UI *_UI) {
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
  lv_obj_t *title =
      ui_create_label(panel, "Today's Forecast", lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t *subtitle =
      ui_create_label(panel, "00:00-24:00, temperature", lv_color_hex(C_MUTED));
  lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 0, 26);
  _UI->forecast_range_24h_btn = ui_create_range_button(
      panel, "24h", UI_RANGE_24H, _UI, forecast_range_event_cb);
  lv_obj_align(_UI->forecast_range_24h_btn, LV_ALIGN_TOP_RIGHT, -140, 0);
  _UI->forecast_range_7d_btn = ui_create_range_button(
      panel, "7d", UI_RANGE_7D, _UI, forecast_range_event_cb);
  lv_obj_align(_UI->forecast_range_7d_btn, LV_ALIGN_TOP_RIGHT, -70, 0);
  _UI->forecast_range_30d_btn = ui_create_range_button(
      panel, "30d", UI_RANGE_30D, _UI, forecast_range_event_cb);
  lv_obj_align(_UI->forecast_range_30d_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
  ui_update_forecast_range_buttons(_UI);
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
  lv_obj_set_size(forecast_view_box, LV_PCT(100), 292);
  lv_obj_align(forecast_view_box, LV_ALIGN_TOP_LEFT, 0, 62);
  lv_obj_set_style_bg_opa(forecast_view_box, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(forecast_view_box, 0, 0);
  lv_obj_set_style_pad_all(forecast_view_box, 0, 0);
  lv_obj_clear_flag(forecast_view_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(forecast_view_box, LV_SCROLLBAR_MODE_OFF);

  _UI->forecast_y_axis_label =
      ui_create_label(forecast_view_box, "Temp C", lv_color_hex(C_MUTED));
  lv_obj_align(_UI->forecast_y_axis_label, LV_ALIGN_TOP_LEFT, 0, 0);

  _UI->forecast_x_axis_label =
      ui_create_label(forecast_view_box, "Time", lv_color_hex(C_MUTED));
  lv_obj_align(_UI->forecast_x_axis_label, LV_ALIGN_BOTTOM_MID, 20, 0);

  _UI->forecast_chart = lv_chart_create(forecast_view_box);
  lv_obj_set_size(_UI->forecast_chart, 780, 250);
  lv_obj_align(_UI->forecast_chart, LV_ALIGN_TOP_LEFT, 62, 8);
  lv_obj_clear_flag(_UI->forecast_chart, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->forecast_chart, LV_SCROLLBAR_MODE_OFF);
  ui_style_chart(_UI->forecast_chart);
  lv_obj_set_style_pad_left(_UI->forecast_chart, 10, LV_PART_TICKS);
  lv_obj_set_style_pad_bottom(_UI->forecast_chart, 8, LV_PART_TICKS);
  lv_obj_set_ext_click_area(_UI->forecast_chart, 8);
  lv_chart_set_type(_UI->forecast_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(_UI->forecast_chart, UI_FORECAST_CHART_POINTS);
  lv_chart_set_range(_UI->forecast_chart, LV_CHART_AXIS_PRIMARY_Y, -200, 300);
  lv_chart_set_axis_tick(_UI->forecast_chart, LV_CHART_AXIS_PRIMARY_X, 4, 2,
                         UI_FORECAST_TIME_TICKS, 1, true, 36);
  lv_chart_set_axis_tick(_UI->forecast_chart, LV_CHART_AXIS_PRIMARY_Y, 4, 2, 5,
                         1, true, 46);
  lv_obj_add_event_cb(_UI->forecast_chart, forecast_axis_draw_cb,
                      LV_EVENT_DRAW_PART_BEGIN, _UI);
  lv_obj_add_event_cb(_UI->forecast_chart, forecast_chart_event_cb,
                      LV_EVENT_VALUE_CHANGED, _UI);
  _UI->forecast_temp_series = lv_chart_add_series(
      _UI->forecast_chart, lv_color_hex(C_BLUE), LV_CHART_AXIS_PRIMARY_Y);

  _UI->forecast_table = lv_table_create(forecast_view_box);
  lv_obj_set_size(_UI->forecast_table, 760, 250);
  lv_obj_align(_UI->forecast_table, LV_ALIGN_TOP_LEFT, 0, 8);
  lv_obj_clear_flag(_UI->forecast_table, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->forecast_table, LV_SCROLLBAR_MODE_OFF);
  lv_table_set_col_cnt(_UI->forecast_table, 5);
  lv_table_set_row_cnt(_UI->forecast_table,
                       UI_FORECAST_TABLE_ROWS_PER_PAGE + 1);
  lv_table_set_cell_value(_UI->forecast_table, 0, 0, "Time");
  lv_table_set_cell_value(_UI->forecast_table, 0, 1, "Temp");
  lv_table_set_cell_value(_UI->forecast_table, 0, 2, "Rain");
  lv_table_set_cell_value(_UI->forecast_table, 0, 3, "Wind");
  lv_table_set_cell_value(_UI->forecast_table, 0, 4, "Solar");
  for (int i = 1; i <= UI_FORECAST_TABLE_ROWS_PER_PAGE; i++) {
    lv_table_set_cell_value(_UI->forecast_table, i, 0, "--:--");
    lv_table_set_cell_value(_UI->forecast_table, i, 1, "--");
    lv_table_set_cell_value(_UI->forecast_table, i, 2, "--");
    lv_table_set_cell_value(_UI->forecast_table, i, 3, "--");
    lv_table_set_cell_value(_UI->forecast_table, i, 4, "--");
  }
  lv_obj_add_flag(_UI->forecast_table, LV_OBJ_FLAG_HIDDEN);

  _UI->forecast_table_prev_btn =
      ui_create_button(forecast_view_box, LV_SYMBOL_UP, lv_color_hex(C_CARD));
  lv_obj_set_size(_UI->forecast_table_prev_btn, 44, 40);
  lv_obj_align(_UI->forecast_table_prev_btn, LV_ALIGN_TOP_RIGHT, 0, 24);
  lv_obj_add_event_cb(_UI->forecast_table_prev_btn,
                      forecast_table_page_event_cb, LV_EVENT_CLICKED, _UI);
  lv_obj_set_user_data(_UI->forecast_table_prev_btn, (void *)(uintptr_t)0);
  lv_obj_add_flag(_UI->forecast_table_prev_btn, LV_OBJ_FLAG_HIDDEN);

  _UI->forecast_table_next_btn =
      ui_create_button(forecast_view_box, LV_SYMBOL_DOWN, lv_color_hex(C_CARD));
  lv_obj_set_size(_UI->forecast_table_next_btn, 44, 40);
  lv_obj_align(_UI->forecast_table_next_btn, LV_ALIGN_BOTTOM_RIGHT, 0, -24);
  lv_obj_add_event_cb(_UI->forecast_table_next_btn,
                      forecast_table_page_event_cb, LV_EVENT_CLICKED, _UI);
  lv_obj_set_user_data(_UI->forecast_table_next_btn, (void *)(uintptr_t)1);
  lv_obj_add_flag(_UI->forecast_table_next_btn, LV_OBJ_FLAG_HIDDEN);

  _UI->forecast_table_page_label =
      ui_create_label(forecast_view_box, "1 / 6", lv_color_hex(C_MUTED));
  lv_obj_align(_UI->forecast_table_page_label, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_flag(_UI->forecast_table_page_label, LV_OBJ_FLAG_HIDDEN);

  if (_UI->has_dashboard_data) {
    ui_update_forecast_data(_UI, &_UI->cached_weather);
  }
}

static void ui_build_screen_elpriser(UI *_UI) {
  _UI->screen_elpriser = lv_obj_create(_UI->content);
  _UI->active_screen = _UI->screen_elpriser;

  lv_obj_set_size(_UI->screen_elpriser, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(_UI->screen_elpriser, lv_color_hex(C_BLACK), 0);
  lv_obj_set_style_border_width(_UI->screen_elpriser, 0, 0);
  lv_obj_set_style_pad_all(_UI->screen_elpriser, 18, 0);
  lv_obj_clear_flag(_UI->screen_elpriser, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(_UI->screen_elpriser, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *panel = ui_create_panel(_UI->screen_elpriser);
  lv_obj_set_size(panel, 940, 455);
  lv_obj_center(panel);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = ui_create_label(panel, "Energy", lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  _UI->energy_range_btn_row = lv_obj_create(panel);
  lv_obj_set_size(_UI->energy_range_btn_row, 188, 32);
  lv_obj_align(_UI->energy_range_btn_row, LV_ALIGN_TOP_RIGHT, 0, 58);
  lv_obj_set_style_bg_opa(_UI->energy_range_btn_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(_UI->energy_range_btn_row, 0, 0);
  lv_obj_set_style_pad_all(_UI->energy_range_btn_row, 0, 0);
  lv_obj_set_style_pad_column(_UI->energy_range_btn_row, 10, 0);
  lv_obj_set_layout(_UI->energy_range_btn_row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(_UI->energy_range_btn_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(_UI->energy_range_btn_row, LV_FLEX_ALIGN_END,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(_UI->energy_range_btn_row, LV_OBJ_FLAG_SCROLLABLE);

  _UI->energy_range_24h_btn = ui_create_range_button(
      _UI->energy_range_btn_row, "24h", UI_RANGE_24H, _UI,
      energy_range_event_cb);
  _UI->energy_range_7d_btn = ui_create_range_button(_UI->energy_range_btn_row,
                                                    "7d", UI_RANGE_7D, _UI,
                                                    energy_range_event_cb);
  _UI->energy_range_30d_btn = ui_create_range_button(
      _UI->energy_range_btn_row, "30d", UI_RANGE_30D, _UI,
      energy_range_event_cb);

  _UI->energy_power_label =
      ui_create_label(panel, "Now: -- W", lv_color_hex(C_GREEN));
  lv_obj_align(_UI->energy_power_label, LV_ALIGN_TOP_RIGHT, 0, 0);

  _UI->energy_max_power_label =
      ui_create_label(panel, "Max 24h: -- W", lv_color_hex(C_MUTED));
  lv_obj_align(_UI->energy_max_power_label, LV_ALIGN_TOP_RIGHT, 0, 28);

  lv_obj_t *kwh_label =
      ui_create_label(panel, "Consumption, kWh", lv_color_hex(C_MUTED));
  lv_obj_align(kwh_label, LV_ALIGN_TOP_LEFT, 0, 54);

  _UI->energy_kwh_chart = lv_chart_create(panel);
  lv_obj_set_size(_UI->energy_kwh_chart, 440, 125);
  lv_obj_align(_UI->energy_kwh_chart, LV_ALIGN_TOP_LEFT, 0, 82);
  ui_configure_energy_chart(_UI, _UI->energy_kwh_chart);
  lv_chart_set_type(_UI->energy_kwh_chart, LV_CHART_TYPE_BAR);
  _UI->energy_kwh_series = lv_chart_add_series(
      _UI->energy_kwh_chart, lv_color_hex(C_GREEN), LV_CHART_AXIS_PRIMARY_Y);
  lv_obj_set_user_data(_UI->energy_kwh_chart,
                       (void *)(uintptr_t)UI_ENERGY_DETAIL_CONSUMPTION);
  lv_obj_add_event_cb(_UI->energy_kwh_chart, energy_chart_event_cb,
                      LV_EVENT_VALUE_CHANGED, _UI);

  lv_obj_t *cost_label =
      ui_create_label(panel, "Cost, SEK", lv_color_hex(C_MUTED));
  lv_obj_align(cost_label, LV_ALIGN_TOP_RIGHT, -300, 54);

  _UI->energy_cost_chart = lv_chart_create(panel);
  lv_obj_set_size(_UI->energy_cost_chart, 440, 125);
  lv_obj_align(_UI->energy_cost_chart, LV_ALIGN_TOP_RIGHT, 0, 82);
  ui_configure_energy_chart(_UI, _UI->energy_cost_chart);
  lv_chart_set_type(_UI->energy_cost_chart, LV_CHART_TYPE_BAR);
  _UI->energy_cost_series = lv_chart_add_series(
      _UI->energy_cost_chart, lv_color_hex(C_YELLOW), LV_CHART_AXIS_PRIMARY_Y);
  lv_obj_set_user_data(_UI->energy_cost_chart,
                       (void *)(uintptr_t)UI_ENERGY_DETAIL_COST);
  lv_obj_add_event_cb(_UI->energy_cost_chart, energy_chart_event_cb,
                      LV_EVENT_VALUE_CHANGED, _UI);

  lv_obj_t *power_label =
      ui_create_label(panel, "Average power, W", lv_color_hex(C_MUTED));
  lv_obj_align(power_label, LV_ALIGN_TOP_LEFT, 0, 242);

  _UI->energy_power_chart = lv_chart_create(panel);
  lv_obj_set_size(_UI->energy_power_chart, LV_PCT(100), 125);
  lv_obj_align(_UI->energy_power_chart, LV_ALIGN_TOP_LEFT, 0, 270);
  ui_configure_energy_chart(_UI, _UI->energy_power_chart);
  lv_chart_set_type(_UI->energy_power_chart, LV_CHART_TYPE_LINE);
  _UI->energy_power_series = lv_chart_add_series(
      _UI->energy_power_chart, lv_color_hex(C_BLUE), LV_CHART_AXIS_PRIMARY_Y);
  lv_obj_set_user_data(_UI->energy_power_chart,
                       (void *)(uintptr_t)UI_ENERGY_DETAIL_POWER);
  lv_obj_add_event_cb(_UI->energy_power_chart, energy_chart_event_cb,
                      LV_EVENT_VALUE_CHANGED, _UI);

  _UI->energy_range_notice_label =
      ui_create_label(panel, "", lv_color_hex(C_MUTED));
  lv_obj_set_width(_UI->energy_range_notice_label, LV_PCT(100));
  lv_label_set_long_mode(_UI->energy_range_notice_label, LV_LABEL_LONG_WRAP);
  lv_obj_align(_UI->energy_range_notice_label, LV_ALIGN_CENTER, 0, 20);
  lv_obj_add_flag(_UI->energy_range_notice_label, LV_OBJ_FLAG_HIDDEN);
  ui_update_energy_range_view(_UI);
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
  lv_obj_t *title = ui_create_label(
      panel, _UI->setup_wizard_active ? "Setup: WiFi" : "WiFi Configuration",
      lv_color_white());
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 70, 0);
  lv_obj_t *sub =
      ui_create_label(panel,
                      _UI->setup_wizard_active
                          ? "Connect this device to a network before continuing"
                          : "Configure network settings",
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

  if (_UI->setup_wizard_active && _UI->wifi_connected) {
    ui_set_wifi_form_status(_UI, "WiFi connected", false);
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

    lv_textarea_set_text(_UI->facility_name_ta,
                         _UI->facility_cfg.facility_name);

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
  if (_UI->setup_wizard_active) {
    lv_obj_add_flag(_UI->back_btn, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_t *title = ui_create_label(
      panel,
      _UI->setup_wizard_active ? "Setup: Facility" : "Facility Configuration",
      lv_color_white());
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
      ui_create_button(panel, _UI->setup_wizard_active ? "Finish" : "Save",
                       lv_color_hex(C_GREEN));
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

  if (_UI->setup_wizard_active) {
    if (_UI->setup_missing_wifi && !_UI->wifi_connected) {
      ui_show_screen(_UI, UI_SCREEN_WIFI);
    } else if (_UI->setup_missing_facility) {
      ui_show_screen(_UI, UI_SCREEN_FACILITY);
    }
    return;
  }

  ui_show_screen(_UI, (UI_Screen)(uintptr_t)lv_obj_get_user_data(btn));
}
static void back_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  if (!_UI)
    return;

  if (_UI->wifi_password_overlay) {
    ui_close_wifi_password(_UI);
    ui_set_wifi_busy(_UI, false);
    wifi_handler_disconnect();
    return;
  }

  if (_UI->setup_wizard_active) {
    _UI->setup_wizard_active = false;
    _UI->setup_missing_wifi = false;
    _UI->setup_missing_facility = false;
    ui_set_footer_text(_UI, "Warning: Wifi/Facility config missing");
    ui_show_screen(_UI, UI_SCREEN_HOME);
    return;
  }

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
    _UI->forecast_view_mode = table ? UI_VIEW_TABLE : UI_VIEW_GRAPH;
    if (table) {
      if (_UI->has_dashboard_data)
        ui_update_forecast_table(_UI, &_UI->cached_weather);
    } else {
      ui_apply_forecast_view(_UI);
    }
    ui_set_range_btn_state(_UI->forecast_graph_btn, !table);
    ui_set_range_btn_state(_UI->forecast_table_btn, table);
  } else if (target == _UI->elpriser_graph_btn ||
             target == _UI->elpriser_table_btn) {
    bool table = target == _UI->elpriser_table_btn;
    _UI->elpriser_view_mode = table ? UI_VIEW_TABLE : UI_VIEW_GRAPH;
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

static void forecast_chart_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  lv_obj_t *chart = lv_event_get_target(_event);
  if (!ui || !chart)
    return;

  if (ui->forecast_range != UI_RANGE_24H)
    return;

  uint32_t point = lv_chart_get_pressed_point(chart);
  if (point == LV_CHART_POINT_NONE || point >= UI_FORECAST_CHART_POINTS)
    return;

  ui_show_forecast_detail(ui, point);
}

static void forecast_range_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  lv_obj_t *btn = lv_event_get_target(_event);
  if (!ui || !btn)
    return;

  ui->forecast_range = (UI_Range)(uintptr_t)lv_obj_get_user_data(btn);
  ui->forecast_table_page = 0;
  ui_update_forecast_data(ui, &ui->cached_weather);
  if (ui->forecast_range == UI_RANGE_24H && ui->forecast_table &&
      !lv_obj_has_flag(ui->forecast_table, LV_OBJ_FLAG_HIDDEN)) {
    if (ui->forecast_table_prev_btn)
      lv_obj_clear_flag(ui->forecast_table_prev_btn, LV_OBJ_FLAG_HIDDEN);
    if (ui->forecast_table_next_btn)
      lv_obj_clear_flag(ui->forecast_table_next_btn, LV_OBJ_FLAG_HIDDEN);
    if (ui->forecast_table_page_label)
      lv_obj_clear_flag(ui->forecast_table_page_label, LV_OBJ_FLAG_HIDDEN);
  }
}

static void energy_range_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  lv_obj_t *btn = lv_event_get_target(_event);
  if (!ui || !btn)
    return;

  ui->energy_range = (UI_Range)(uintptr_t)lv_obj_get_user_data(btn);
  ui_update_energy_range_view(ui);
  dashboard_data_request_energy_range(
      (DashboardEnergyRange)ui->energy_range);
}

static void forecast_detail_close_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  ui_close_forecast_detail(ui);
}

static void forecast_detail_page_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  lv_obj_t *btn = lv_event_get_target(_event);
  if (!ui || !btn)
    return;

  int page = (int)(uintptr_t)lv_obj_get_user_data(btn);
  ui_set_forecast_detail_page(ui, page);
}

static void forecast_table_page_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  lv_obj_t *btn = lv_event_get_target(_event);
  if (!ui || !btn)
    return;

  int direction = (int)(uintptr_t)lv_obj_get_user_data(btn);
  if (direction == 0)
    ui->forecast_table_page--;
  else
    ui->forecast_table_page++;

  ui_update_forecast_table(ui, &ui->cached_weather);
}

static void energy_chart_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  lv_obj_t *chart = lv_event_get_target(_event);
  if (!ui || !chart)
    return;

  uint32_t point = lv_chart_get_pressed_point(chart);
  if (point == LV_CHART_POINT_NONE || point >= (uint32_t)ui_energy_point_count(ui))
    return;

  UI_EnergyDetailKind kind =
      (UI_EnergyDetailKind)(uintptr_t)lv_obj_get_user_data(chart);
  ui_show_energy_detail(ui, kind, point);
}

static void energy_detail_close_event_cb(lv_event_t *_event) {
  UI *ui = lv_event_get_user_data(_event);
  ui_close_energy_detail(ui);
}

static void energy_detail_info_event_cb(lv_event_t *_event) {
  lv_obj_t *btn = lv_event_get_target(_event);
  lv_obj_t *info_box = btn ? lv_obj_get_user_data(btn) : NULL;
  if (!info_box)
    return;

  if (lv_obj_has_flag(info_box, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_clear_flag(info_box, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(info_box, LV_OBJ_FLAG_HIDDEN);
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

  _UI->wifi_show_password_cb = lv_checkbox_create(_UI->wifi_password_panel);
  lv_checkbox_set_text(_UI->wifi_show_password_cb, "Show password");
  lv_obj_set_style_text_color(_UI->wifi_show_password_cb, lv_color_white(), 0);
  lv_obj_align(_UI->wifi_show_password_cb, LV_ALIGN_TOP_LEFT, 0, 104);
  lv_obj_add_event_cb(_UI->wifi_show_password_cb, wifi_show_password_event_cb,
                      LV_EVENT_VALUE_CHANGED, _UI);

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
    _UI->wifi_show_password_cb = NULL;
  }
}

static void wifi_cancel_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  ui_set_wifi_busy(_UI, false);
  ui_close_wifi_password(_UI);
  wifi_handler_disconnect();
}

static void wifi_show_password_event_cb(lv_event_t *_event) {
  UI *_UI = lv_event_get_user_data(_event);
  lv_obj_t *cb = lv_event_get_target(_event);
  if (!_UI || !_UI->wifi_pass_ta || !cb)
    return;

  bool show = lv_obj_has_state(cb, LV_STATE_CHECKED);
  lv_textarea_set_password_mode(_UI->wifi_pass_ta, !show);
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

  if (_UI && _UI->facility_page < 1) {
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

  bool complete = ui_facility_config_complete(&_UI->facility_cfg);
  esp_err_t err = complete ? facility_config_set_all(&_UI->facility_cfg)
                           : ESP_ERR_INVALID_ARG;

  if (_UI->facility_save_btn) {
    lv_obj_clear_state(_UI->facility_save_btn, LV_STATE_DISABLED);
  }

  if (_UI->facility_status_label) {
    if (err == ESP_OK) {
      lv_label_set_text(_UI->facility_status_label, "Saved facility");
      lv_obj_set_style_text_color(_UI->facility_status_label,
                                  lv_color_hex(C_GREEN), 0);
      ui_setup_finish_if_complete(_UI);
    } else if (!complete) {
      lv_label_set_text(_UI->facility_status_label,
                        "Complete all required fields");
      lv_obj_set_style_text_color(_UI->facility_status_label,
                                  lv_color_hex(C_RED), 0);
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
  if (_connected && _UI->wifi_password_overlay) {
    ui_close_wifi_password(_UI);
  }

  if (_UI->current_screen == UI_SCREEN_WIFI) {
    if (_UI->setup_wizard_active && _connected) {
      if (_UI->setup_missing_facility) {
        ui_show_screen(_UI, UI_SCREEN_FACILITY);
      } else {
        ui_setup_finish_if_complete(_UI);
      }
    } else if (_connected) {
      ui_show_screen(_UI, UI_SCREEN_WIFI);
    }
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

void ui_start_setup_wizard(UI *_UI, bool missing_wifi, bool missing_facility) {
  if (!_UI || (!missing_wifi && !missing_facility))
    return;

  _UI->setup_wizard_active = true;
  _UI->setup_missing_wifi = missing_wifi;
  _UI->setup_missing_facility = missing_facility;

  ui_set_footer_text(_UI, "Setup required");

  if (missing_wifi && !_UI->wifi_connected) {
    ui_show_screen(_UI, UI_SCREEN_WIFI);
    ui_set_wifi_form_status(_UI, "Scanning for WiFi networks...", false);
    if (_UI->wifi_network_count <= 0 && wifi_handler_scan() != ESP_OK) {
      ui_set_wifi_form_status(_UI, "Failed to start WiFi scan", true);
    }
    return;
  }

  if (missing_facility) {
    ui_show_screen(_UI, UI_SCREEN_FACILITY);
    return;
  }

  ui_setup_finish_if_complete(_UI);
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

  _UI->current_hour = h;
  _UI->current_minute = m;
  _UI->has_time = true;

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

  if (_UI->energy_kwh_chart)
    lv_obj_invalidate(_UI->energy_kwh_chart);
  if (_UI->energy_cost_chart)
    lv_obj_invalidate(_UI->energy_cost_chart);
  if (_UI->energy_power_chart)
    lv_obj_invalidate(_UI->energy_power_chart);
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
