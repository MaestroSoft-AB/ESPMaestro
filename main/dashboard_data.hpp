#pragma once
#include "dashboard_types.h"
#include "dashboard_data_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "scheduler.h"
#include <stdint.h>

typedef enum {
  DBD_STATE_INIT,
  DBD_STATE_IDLE,
  DBD_STATE_REQUEST_DATA,
  DBD_STATE_WAIT_RESPONSE,
  DBD_STATE_PARSE_DATA,
  DBD_STATE_UPDATE_GRAPHS
} DashboardDataStatus;

class DashboardData {
private:
  WeatherData weatherdata_;
  ElectricityData electricitydata_;
  RealtimeData realtimedata_;
  bool initialized_;

public:
  DashboardDataStatus state;
  bool need_weaterdata;
  bool need_electricitydata;
  bool need_realtimedata;

  Scheduler_Task *task;
  TaskHandle_t fetch_task;
  QueueHandle_t fetch_result_queue;
  bool fetch_in_progress;
  bool pending_range_refresh_;
  DashboardEnergyRange energy_range_;
  uint64_t base_epoch;
  uint64_t base_ms;
  uint64_t next_fetch_ms;

  DashboardData();

  void update_weather(float outdoor_c, float indoor_c, const char *summary);

  void update_weather_forecast(float outdoor_c, const char *summary,
                               const float temp_c_24h[24],
                               const uint8_t rain_percent_24h[24],
                               const uint16_t weather_code_24h[24],
                               const uint16_t shortwave_wm2_24h[24],
                               const float wind_kmh_24h[24],
                               const char time_24h[24][6]);

  void update_electricity(float current_sek_kwh,
                          const float sek_24h[DASHBOARD_ENERGY_MAX_POINTS],
                          uint8_t point_count, uint16_t interval_minutes,
                          const char labels[DASHBOARD_ENERGY_MAX_POINTS][6],
                          const bool has_data[DASHBOARD_ENERGY_MAX_POINTS]);

  void update_realtime(uint32_t power_w, uint32_t max_power_w_24h,
                       float current_kwh, float current_sek_h,
                       const uint32_t power_24h[DASHBOARD_ENERGY_MAX_POINTS],
                       const float kwh_24h[DASHBOARD_ENERGY_MAX_POINTS],
                       const float cost_24h[DASHBOARD_ENERGY_MAX_POINTS],
                       uint8_t point_count, uint16_t interval_minutes,
                       const char labels[DASHBOARD_ENERGY_MAX_POINTS][6],
                       const bool has_data[DASHBOARD_ENERGY_MAX_POINTS]);

  void request_energy_range(DashboardEnergyRange range);

  void send_to_display_handler();

  // Prohibit copy
  DashboardData(const DashboardData &) = delete;
  DashboardData &operator=(const DashboardData &) = delete;

  // Prohibit move
  DashboardData(DashboardData &&) = delete;
  DashboardData &operator=(DashboardData &&) = delete;

  ~DashboardData();
};
