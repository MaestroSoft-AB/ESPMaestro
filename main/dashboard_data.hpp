#pragma once
#include "scheduler.h"
#include <stdint.h>

typedef enum {
  DBD_STATUS_INIT,
  DBD_STATUS_IDLE,
  DBD_STATUS_REQUEST_DATA,
  DBD_STATUS_UPDATE_GRAPH
} DashboardDataStatus;

struct WeatherData {
  bool valid = false;
  float outdoor_c = 0.0f;
  float indoor_c = 0.0f;
  char summary[32] = "";
  uint32_t updated_epoch = 0;
};

struct ElectricityData {
  bool valid = false;
  float current_sek_kwh = 0.0f;
  float sek_24h[24] = {};
  uint32_t updated_epoch = 0;
};

struct RealtimeData {
  bool valid = false;
  uint32_t power_w = 0;
  uint32_t max_power_w_24h = 0;
  float current_kwh = 0.0f;
  float current_sek_h = 0.0f;
  uint32_t power_24h[24] = {};
  float kwh_24h[24] = {};
  float cost_24h[24] = {};
  uint32_t updated_epoch = 0;
};

class DashboardData {
private:
  WeatherData weatherdata_;
  ElectricityData electricitydata_;
  RealtimeData realtimedata_;

public:
  DashboardDataStatus state;
  bool need_weaterdata;
  bool need_electricitydata;
  bool need_realtimedata;

  uint64_t base_epoch_;
  uint64_t base_ms_;
  uint64_t next_weather_ms_;
  uint64_t next_electricity_ms_;
  uint64_t next_realtime_ms_;

  DashboardData();

  void update_weather(float outdoor_c, float indoor_c, const char *summary);

  void update_electricity(float current_sek_kwh, const float sek_24h[24]);

  void update_realtime(uint32_t power_w, uint32_t max_power_w_24h,
                       float current_kwh, float current_sek_h,
                       const uint32_t power_24h[24], const float kwh_24h[24],
                       const float cost_24h[24]);

  // Prohibit copy
  DashboardData(const DashboardData &) = delete;
  DashboardData &operator=(const DashboardData &) = delete;

  // Prohibit move
  DashboardData(DashboardData &&) = delete;
  DashboardData &operator=(DashboardData &&) = delete;

  ~DashboardData();
};
