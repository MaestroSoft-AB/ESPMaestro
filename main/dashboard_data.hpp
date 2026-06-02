#pragma once
#include "dashboard_types.h"
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
  uint64_t base_epoch;
  uint64_t base_ms;
  uint64_t next_fetch_ms;

  DashboardData();

  void update_weather(float outdoor_c, float indoor_c, const char *summary);

  void update_electricity(float current_sek_kwh, const float sek_24h[24]);

  void update_realtime(uint32_t power_w, uint32_t max_power_w_24h,
                       float current_kwh, float current_sek_h,
                       const uint32_t power_24h[24], const float kwh_24h[24],
                       const float cost_24h[24]);

  void send_to_display_handler();

  // Prohibit copy
  DashboardData(const DashboardData &) = delete;
  DashboardData &operator=(const DashboardData &) = delete;

  // Prohibit move
  DashboardData(DashboardData &&) = delete;
  DashboardData &operator=(DashboardData &&) = delete;

  ~DashboardData();
};
