#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool valid;
  float outdoor_c;
  float indoor_c;
  char summary[32];
  uint32_t updated_epoch;
} WeatherData;

typedef struct {
  bool valid;
  float current_sek_kwh;
  float sek_24h[24];
  uint32_t updated_epoch;
} ElectricityData;

typedef struct {
  bool valid;
  uint32_t power_w;
  uint32_t max_power_w_24h;
  float current_kwh;
  float current_sek_h;

  uint32_t power_24h[24];
  float kwh_24h[24];
  float cost_24h[24];

  uint32_t updated_epoch;
} RealtimeData;
