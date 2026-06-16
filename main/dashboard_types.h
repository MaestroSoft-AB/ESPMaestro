#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DASHBOARD_ENERGY_MAX_POINTS 30
#define DASHBOARD_WEATHER_HOURLY_POINTS 25
#define DASHBOARD_ERROR_TEXT_MAX 96

typedef struct {
  bool valid;
  float outdoor_c;
  float indoor_c;
  char summary[32];
  float temp_c_24h[DASHBOARD_WEATHER_HOURLY_POINTS];
  uint8_t rain_percent_24h[DASHBOARD_WEATHER_HOURLY_POINTS];
  uint16_t weather_code_24h[DASHBOARD_WEATHER_HOURLY_POINTS];
  uint16_t shortwave_wm2_24h[DASHBOARD_WEATHER_HOURLY_POINTS];
  float wind_kmh_24h[DASHBOARD_WEATHER_HOURLY_POINTS];
  char time_24h[DASHBOARD_WEATHER_HOURLY_POINTS][6];
  char last_error[DASHBOARD_ERROR_TEXT_MAX];
  uint32_t updated_epoch;
} WeatherData;

typedef struct {
  bool valid;
  float current_sek_kwh;
  float avg_sek_kwh_day;
  float sek_24h[DASHBOARD_ENERGY_MAX_POINTS];
  uint8_t point_count;
  uint16_t interval_minutes;
  char labels[DASHBOARD_ENERGY_MAX_POINTS][6];
  bool has_data[DASHBOARD_ENERGY_MAX_POINTS];
  char last_error[DASHBOARD_ERROR_TEXT_MAX];
  uint32_t updated_epoch;
} ElectricityData;

typedef struct {
  bool valid;
  uint32_t power_w;
  uint32_t historical_avg_power_w;
  uint32_t max_power_w_24h;
  float current_kwh;
  float current_sek_h;

  uint32_t power_24h[DASHBOARD_ENERGY_MAX_POINTS];
  float kwh_24h[DASHBOARD_ENERGY_MAX_POINTS];
  float cost_24h[DASHBOARD_ENERGY_MAX_POINTS];
  uint8_t point_count;
  uint16_t interval_minutes;
  char labels[DASHBOARD_ENERGY_MAX_POINTS][6];
  bool has_data[DASHBOARD_ENERGY_MAX_POINTS];

  char last_error[DASHBOARD_ERROR_TEXT_MAX];
  uint32_t updated_epoch;
} RealtimeData;
