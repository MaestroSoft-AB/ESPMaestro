#pragma once
#include "dashboard_data_api.h"
#include "dashboard_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "scheduler.h"
#include <stdint.h>

/**
 * @brief State machine states used by DashboardData.
 */
typedef enum {
  /** @brief Initial state. */
  DBD_STATE_INIT,

  /** @brief Waiting for the next scheduled fetch. */
  DBD_STATE_IDLE,

  /** @brief Requesting new dashboard data. */
  DBD_STATE_REQUEST_DATA,

  /** @brief Waiting for the background fetch task to complete. */
  DBD_STATE_WAIT_RESPONSE,

  /** @brief Reserved state for parsing fetched data. */
  DBD_STATE_PARSE_DATA,

  /** @brief Updating display graphs and dashboard widgets. */
  DBD_STATE_UPDATE_GRAPHS
} DashboardDataStatus;

/**
 * @brief Central dashboard data manager.
 *
 * This class stores weather, electricity price, and realtime power
 * consumption data used by the display dashboard.
 *
 * It owns a scheduler task and a background FreeRTOS worker task that
 * periodically retrieves electricity and consumption statistics from
 * remote services, updates internal data structures, and forwards
 * the latest values to the display handler.
 *
 * Copying and moving are disabled because the class owns FreeRTOS
 * resources such as tasks and queues.
 */
class DashboardData {
private:
  /** @brief Latest weather information. */
  WeatherData weatherdata_;

  /** @brief Latest electricity price information. */
  ElectricityData electricitydata_;

  /** @brief Latest realtime power consumption information. */
  RealtimeData realtimedata_;

  /** @brief True when initialization completed successfully. */
  bool initialized_;

public:
  /** @brief Current dashboard state machine state. */
  DashboardDataStatus state;

  /** @brief Indicates that weather data should be refreshed. */
  bool need_weatherdata;

  /** @brief Indicates that electricity pricing data should be refreshed. */
  bool need_electricitydata;

  /** @brief Indicates that realtime consumption data should be refreshed. */
  bool need_realtimedata;

  /** @brief Scheduler task controlling dashboard updates. */
  Scheduler_Task *task;

  /** @brief Background worker task used for HTTP data retrieval. */
  TaskHandle_t fetch_task;

  /** @brief Queue used to transfer fetch results from worker to state machine.
   */
  QueueHandle_t fetch_result_queue;

  /** @brief True while a fetch operation is currently running. */
  bool fetch_in_progress;
  bool pending_range_refresh_;
  bool pending_manual_refresh_;
  DashboardEnergyRange energy_range_;
  uint64_t manual_refresh_ms_;

  /** @brief Reference Unix epoch used for local time calculations. */
  uint64_t base_epoch;

  /** @brief Millisecond timestamp corresponding to base_epoch. */
  uint64_t base_ms;

  /** @brief Timestamp for the next scheduled data fetch. */
  uint64_t next_fetch_ms;

  /**
   * @brief Construct and initialize the dashboard manager.
   *
   * Creates the fetch result queue, starts the background worker task,
   * and registers a scheduler task used to drive the dashboard state machine.
   */
  DashboardData();

  /**
   * @brief Update cached weather information.
   *
   * Marks the weather data as valid and updates the last update timestamp.
   *
   * @param outdoor_c Outdoor temperature in degrees Celsius.
   * @param indoor_c Indoor temperature in degrees Celsius.
   * @param summary Short weather summary text.
   */
  void update_weather(float outdoor_c, float indoor_c, const char *summary);
  void set_weather_error(const char *error);

  void update_weather_forecast(float outdoor_c, const char *summary,
                               const float temp_c_24h[DASHBOARD_WEATHER_HOURLY_POINTS],
                               const uint8_t rain_percent_24h[DASHBOARD_WEATHER_HOURLY_POINTS],
                               const uint16_t weather_code_24h[DASHBOARD_WEATHER_HOURLY_POINTS],
                               const uint16_t shortwave_wm2_24h[DASHBOARD_WEATHER_HOURLY_POINTS],
                               const float wind_kmh_24h[DASHBOARD_WEATHER_HOURLY_POINTS],
                               const char time_24h[DASHBOARD_WEATHER_HOURLY_POINTS][6],
                               const float temp_c_daily[DASHBOARD_WEATHER_DAILY_POINTS],
                               const uint8_t rain_percent_daily[DASHBOARD_WEATHER_DAILY_POINTS],
                               const uint16_t weather_code_daily[DASHBOARD_WEATHER_DAILY_POINTS],
                               const float wind_kmh_daily[DASHBOARD_WEATHER_DAILY_POINTS],
                               const char time_daily[DASHBOARD_WEATHER_DAILY_POINTS][6],
                               uint8_t daily_count);

  /**
   * @brief Update cached electricity pricing data.
   *
   * @param current_sek_kwh Current electricity price in SEK/kWh.
   * @param sek_24h Array containing hourly electricity prices for the last 24
   * hours.
   */
  void update_electricity(float current_sek_kwh, float avg_sek_kwh_day,
                          const float sek_24h[DASHBOARD_ENERGY_MAX_POINTS],
                          uint8_t point_count, uint16_t interval_minutes,
                          const char labels[DASHBOARD_ENERGY_MAX_POINTS][6],
                          const bool has_data[DASHBOARD_ENERGY_MAX_POINTS]);
  void set_energy_error(const char *error);

  /**
   * @brief Update cached realtime energy consumption data.
   *
   * @param power_w Current power consumption in watts.
   * @param max_power_w_24h Highest recorded power consumption during the last
   * 24 hours.
   * @param current_kwh Total energy consumption.
   * @param current_sek_h Total electricity cost.
   * @param power_24h Hourly power profile.
   * @param kwh_24h Hourly energy consumption profile.
   * @param cost_24h Hourly cost profile.
   */
  void update_realtime(uint32_t power_w, uint32_t historical_avg_power_w,
                       uint32_t max_power_w_24h, float current_kwh,
                       float current_sek_h,
                       const uint32_t power_24h[DASHBOARD_ENERGY_MAX_POINTS],
                       const float kwh_24h[DASHBOARD_ENERGY_MAX_POINTS],
                       const float cost_24h[DASHBOARD_ENERGY_MAX_POINTS],
                       uint8_t point_count, uint16_t interval_minutes,
                       const char labels[DASHBOARD_ENERGY_MAX_POINTS][6],
                       const bool has_data[DASHBOARD_ENERGY_MAX_POINTS]);

  void request_energy_range(DashboardEnergyRange range);
  void request_refresh(uint32_t delay_ms);

  /**
   * @brief Send the currently cached dashboard data to the display handler.
   *
   * This updates the user interface with the latest weather,
   * electricity, and realtime consumption information.
   */
  void send_to_display_handler();

  // Prohibit copy
  DashboardData(const DashboardData &) = delete;
  DashboardData &operator=(const DashboardData &) = delete;

  // Prohibit move
  DashboardData(DashboardData &&) = delete;
  DashboardData &operator=(DashboardData &&) = delete;

  /**
   * @brief Destroy the dashboard manager.
   *
   * Stops and deletes owned scheduler tasks, worker tasks,
   * and communication queues.
   */
  ~DashboardData();
};
