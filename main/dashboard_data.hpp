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
 *
 * Normal progression after construction:
 * DBD_STATE_INIT → DBD_STATE_IDLE → DBD_STATE_REQUEST_DATA →
 * DBD_STATE_WAIT_RESPONSE → DBD_STATE_UPDATE_GRAPHS → DBD_STATE_IDLE
 *
 * The cycle repeats on the next aligned fetch deadline. On fetch failure
 * the state returns to DBD_STATE_IDLE with a shorter retry interval
 * (OPTIMAESTRO_FETCH_RETRY_MS) instead of the full aligned interval.
 */
typedef enum {
  /**
   * @brief Initial state entered on construction.
   *
   * Sets next_fetch_ms to now and transitions immediately to
   * DBD_STATE_IDLE on the first scheduler tick.
   */
  DBD_STATE_INIT,

  /**
   * @brief Idle state. Waits until next_fetch_ms is reached.
   *
   * Transitions to DBD_STATE_REQUEST_DATA when the current monotonic
   * time exceeds next_fetch_ms.
   */
  DBD_STATE_IDLE,

  /**
   * @brief Triggers a background HTTP fetch.
   *
   * Verifies Wi-Fi connectivity, flushes any stale result from the queue,
   * sets fetch_in_progress, and notifies the fetch worker task via
   * xTaskNotifyGive(). If Wi-Fi is not connected, schedules a retry after
   * OPTIMAESTRO_FETCH_RETRY_MS and returns to DBD_STATE_IDLE.
   * Transitions to DBD_STATE_WAIT_RESPONSE on success.
   */
  DBD_STATE_REQUEST_DATA,

  /**
   * @brief Polls the fetch result queue for a completed response.
   *
   * Returns to itself until a DbdFetchResult is available. On receipt,
   * updates internal data via update_electricity(), update_realtime(), and
   * update_weather_forecast() as appropriate, then transitions to
   * DBD_STATE_UPDATE_GRAPHS. If a range change or manual refresh is pending,
   * may transition directly back to DBD_STATE_REQUEST_DATA.
   */
  DBD_STATE_WAIT_RESPONSE,

  /**
   * @brief Reserved for future parsed-data processing.
   *
   * Currently unused; parsing is performed inline in DBD_STATE_WAIT_RESPONSE.
   */
  DBD_STATE_PARSE_DATA,

  /**
   * @brief Forwards the latest cached data to the display handler.
   *
   * Calls send_to_display_handler() then transitions to DBD_STATE_IDLE.
   */
  DBD_STATE_UPDATE_GRAPHS
} DashboardDataStatus;

/**
 * @brief Central dashboard data manager.
 *
 * Owns and coordinates three subsystems:
 * - A scheduler task that drives the fetch state machine (dbd_taskwork).
 * - A background FreeRTOS worker task (dbd_fetch_worker_task) that performs
 *   blocking HTTP requests to the OptiMaestro backend for energy and weather
 *   data, communicating results back via a depth-1 FreeRTOS queue.
 * - An NVS cache under the "dash_cache" namespace that persists the last
 *   known outdoor temperature, electricity price, and live power so that
 *   something meaningful can be shown on the display immediately at boot.
 *
 * The state machine fetches on a wall-clock-aligned schedule
 * (OPTIMAESTRO_REFRESH_PERIOD_SEC, offset by OPTIMAESTRO_REFRESH_OFFSET_SEC)
 * or falls back to OPTIMAESTRO_FETCH_RETRY_MS on failure or when NTP is not
 * yet available.
 *
 * A global singleton pointer (g_dashboard_data_instance) is set in the
 * constructor so that the C API functions dashboard_data_request_refresh()
 * and dashboard_data_request_energy_range() can reach the instance.
 *
 * Copying and moving are disabled because the class owns FreeRTOS tasks and
 * queues.
 */
class DashboardData {
private:
  /** @brief Latest weather data forwarded to the display handler. */
  WeatherData weatherdata_;

  /** @brief Latest electricity price data forwarded to the display handler. */
  ElectricityData electricitydata_;

  /** @brief Latest realtime power consumption data forwarded to the display
   * handler. */
  RealtimeData realtimedata_;

  /**
   * @brief True if construction fully succeeded.
   *
   * Checked by the destructor to decide whether to call
   * scheduler_destroy_task(). Set to true only after the queue, worker task,
   * and scheduler task are all created successfully.
   */
  bool initialized_;

public:
  /** @brief Current state machine state, driven by dbd_taskwork(). */
  DashboardDataStatus state;

  /** @brief True when weather data should be included in the next fetch. */
  bool need_weatherdata;

  /** @brief True when electricity pricing data should be included in the next
   * fetch. */
  bool need_electricitydata;

  /** @brief True when realtime consumption data should be included in the next
   * fetch. */
  bool need_realtimedata;

  /** @brief Scheduler task that drives the dashboard state machine. */
  Scheduler_Task *task;

  /**
   * @brief Background FreeRTOS worker task for blocking HTTP retrieval.
   *
   * Woken by xTaskNotifyGive() from DBD_STATE_REQUEST_DATA. Performs both
   * the energy and weather HTTP fetches, then writes the result to
   * fetch_result_queue via xQueueOverwrite().
   */
  TaskHandle_t fetch_task;

  /**
   * @brief Depth-1 queue used to transfer DbdFetchResult from the worker task
   * to the state machine.
   *
   * xQueueOverwrite() is used by the worker so that a stale result is never
   * blocking a new fetch cycle.
   */
  QueueHandle_t fetch_result_queue;

  /**
   * @brief True while the worker task is executing a fetch.
   *
   * Set to true in DBD_STATE_REQUEST_DATA and cleared in
   * DBD_STATE_WAIT_RESPONSE when a result is received.
   */
  bool fetch_in_progress;

  /**
   * @brief True when an energy range change arrived while a fetch was in
   * progress.
   *
   * Causes DBD_STATE_WAIT_RESPONSE to immediately re-enter
   * DBD_STATE_REQUEST_DATA after the current result is processed.
   */
  bool pending_range_refresh_;

  /**
   * @brief True when request_refresh() was called while a fetch was in
   * progress.
   *
   * The scheduler will re-fetch at manual_refresh_ms_ rather than waiting for
   * the next aligned interval.
   */
  bool pending_manual_refresh_;

  /** @brief Currently active energy time range used in fetch requests. */
  DashboardEnergyRange energy_range_;

  /**
   * @brief Target monotonic ms timestamp for a deferred manual refresh.
   *
   * Set by request_refresh() when fetch_in_progress is true. Consumed by
   * DBD_STATE_WAIT_RESPONSE once the current fetch completes.
   */
  uint64_t manual_refresh_ms_;

  /**
   * @brief Unix epoch recorded at the last NTP synchronization.
   *
   * Reserved for local time derivation; currently populated but not actively
   * used by the state machine (time() is called directly in the worker).
   */
  uint64_t base_epoch;

  /** @brief Monotonic ms timestamp corresponding to base_epoch. */
  uint64_t base_ms;

  /**
   * @brief Monotonic ms deadline for the next scheduled data fetch.
   *
   * Set to 0 on construction so the first fetch fires immediately. Updated
   * by dbd_next_aligned_fetch_ms() after a successful fetch, or set to
   * now + OPTIMAESTRO_FETCH_RETRY_MS on failure.
   */
  uint64_t next_fetch_ms;

  /**
   * @brief Construct and initialize the dashboard manager.
   *
   * Performs the following in order:
   * 1. Creates a depth-1 fetch result queue.
   * 2. Starts the background HTTP worker task (dbd_fetch_worker_task).
   * 3. Registers a scheduler task (dbd_taskwork) to drive the state machine.
   * 4. Loads cached weather, electricity, and power values from NVS and
   *    forwards them to the display handler so the UI shows something
   *    immediately at boot.
   *
   * If any of steps 1–3 fail, previously created resources are cleaned up
   * and initialized_ is set to false.
   */
  DashboardData();

  /**
   * @brief Destroy the dashboard manager.
   *
   * Destroys the scheduler task (if initialized_), deletes the worker task,
   * and deletes the fetch result queue.
   */
  ~DashboardData();

  /** @brief Copy construction disabled — object owns FreeRTOS resources. */
  DashboardData(const DashboardData &) = delete;

  /** @brief Copy assignment disabled — object owns FreeRTOS resources. */
  DashboardData &operator=(const DashboardData &) = delete;

  /** @brief Move construction disabled — object owns FreeRTOS resources. */
  DashboardData(DashboardData &&) = delete;

  /** @brief Move assignment disabled — object owns FreeRTOS resources. */
  DashboardData &operator=(DashboardData &&) = delete;

  /**
   * @brief Update cached weather data and persist outdoor temperature to NVS.
   *
   * Marks weatherdata_ valid, clears any previous error string, and saves
   * outdoor_c to NVS under "outdoor_c". Updates the updated_epoch timestamp.
   *
   * @param outdoor_c Outdoor temperature in degrees Celsius.
   * @param indoor_c  Indoor temperature in degrees Celsius (passed through
   *                  from the BME280 reading stored elsewhere).
   * @param summary   Short weather summary string (e.g. "Partly cloudy").
   *                  Null is treated as an empty string.
   */
  void update_weather(float outdoor_c, float indoor_c, const char *summary);

  /**
   * @brief Store a weather error string in the cached weather data.
   *
   * Updates last_error in weatherdata_ and refreshes updated_epoch. Does not
   * clear or invalidate previously cached weather values.
   *
   * @param error Null-terminated error description.
   */
  void set_weather_error(const char *error);

  /**
   * @brief Update cached weather data with full hourly forecast arrays.
   *
   * Calls update_weather() for the scalar fields, then copies all hourly
   * forecast arrays into weatherdata_. The indoor temperature is preserved
   * from the existing weatherdata_.indoor_c.
   *
   * @param outdoor_c          Current outdoor temperature in degrees Celsius.
   * @param summary            Short weather summary string.
   * @param temp_c_24h         Hourly temperature forecast (°C).
   * @param rain_percent_24h   Hourly precipitation probability (0–100).
   * @param weather_code_24h   Hourly WMO weather interpretation codes.
   * @param shortwave_wm2_24h  Hourly global horizontal irradiance (W/m²).
   * @param wind_kmh_24h       Hourly wind speed (km/h).
   * @param time_24h           Hourly label strings ("HH:MM", or "Now" for
   *                           index 0).
   */
  void update_weather_forecast(
      float outdoor_c, const char *summary,
      const float temp_c_24h[DASHBOARD_WEATHER_HOURLY_POINTS],
      const uint8_t rain_percent_24h[DASHBOARD_WEATHER_HOURLY_POINTS],
      const uint16_t weather_code_24h[DASHBOARD_WEATHER_HOURLY_POINTS],
      const uint16_t shortwave_wm2_24h[DASHBOARD_WEATHER_HOURLY_POINTS],
      const float wind_kmh_24h[DASHBOARD_WEATHER_HOURLY_POINTS],
      const char time_24h[DASHBOARD_WEATHER_HOURLY_POINTS][6]);

  /**
   * @brief Update cached electricity pricing data and persist current price to
   * NVS.
   *
   * Marks electricitydata_ valid, clears any previous error string, copies all
   * supplied arrays, and saves current_sek_kwh to NVS under "price". Updates
   * updated_epoch.
   *
   * @param current_sek_kwh    Current spot price in SEK/kWh.
   * @param avg_sek_kwh_day    Average spot price over the displayed period.
   * @param sek_24h            Per-point spot prices in SEK/kWh.
   * @param point_count        Number of valid points in the arrays.
   * @param interval_minutes   Time interval represented by each data point.
   * @param labels             Per-point time label strings (e.g. "14:00").
   * @param has_data           Per-point flag indicating whether data is
   *                           available (false = gap in series).
   */
  void update_electricity(float current_sek_kwh, float avg_sek_kwh_day,
                          const float sek_24h[DASHBOARD_ENERGY_MAX_POINTS],
                          uint8_t point_count, uint16_t interval_minutes,
                          const char labels[DASHBOARD_ENERGY_MAX_POINTS][6],
                          const bool has_data[DASHBOARD_ENERGY_MAX_POINTS]);

  /**
   * @brief Store an energy error string in both electricity and realtime cached
   * data.
   *
   * Sets last_error on both electricitydata_ and realtimedata_ and refreshes
   * both updated_epoch fields. Does not clear previously cached values.
   *
   * @param error Null-terminated error description.
   */
  void set_energy_error(const char *error);

  /**
   * @brief Update cached realtime energy consumption data and persist live
   * power to NVS.
   *
   * Marks realtimedata_ valid, clears any previous error string, copies all
   * supplied arrays, and saves power_w to NVS under "power_w". Updates
   * updated_epoch.
   *
   * @param power_w                Current instantaneous power in watts.
   * @param historical_avg_power_w Historical average power for the current
   *                               15-minute bucket in watts.
   * @param max_power_w_24h        Peak power recorded in the displayed period.
   * @param current_kwh            Cumulative energy for the displayed period
   *                               in kWh.
   * @param current_sek_h          Cumulative electricity cost for the
   *                               displayed period in SEK.
   * @param power_24h              Per-point average power in watts.
   * @param kwh_24h                Per-point energy in kWh.
   * @param cost_24h               Per-point electricity cost in SEK.
   * @param point_count            Number of valid points in the arrays.
   * @param interval_minutes       Time interval represented by each data point.
   * @param labels                 Per-point time label strings.
   * @param has_data               Per-point data availability flags.
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

  /**
   * @brief Switch the displayed energy time range and trigger a re-fetch.
   *
   * Updates energy_range_ and resets next_fetch_ms to 0 so the state machine
   * fetches immediately on the next tick. If a fetch is already in progress
   * and the range differs, sets pending_range_refresh_ so the result is
   * discarded and a new fetch is triggered automatically.
   *
   * @param range New energy time range to display.
   */
  void request_energy_range(DashboardEnergyRange range);

  /**
   * @brief Schedule or accelerate a dashboard data refresh.
   *
   * Computes a target monotonic timestamp of now + @p delay_ms. If no fetch
   * is currently in progress and the target is earlier than the current
   * next_fetch_ms, next_fetch_ms is updated immediately. If a fetch is in
   * progress, the request is deferred: pending_manual_refresh_ is set and
   * manual_refresh_ms_ records the earliest requested target, which is applied
   * by DBD_STATE_WAIT_RESPONSE once the in-progress fetch completes.
   *
   * @param delay_ms Minimum delay in milliseconds before the refresh occurs.
   *                 Pass 0 for an immediate refresh (subject to fetch_in_
   *                 progress deferral).
   */
  void request_refresh(uint32_t delay_ms);

  /**
   * @brief Forward the current cached data to the display handler.
   *
   * Calls display_handler_update_dashboard() with pointers to weatherdata_,
   * electricitydata_, and realtimedata_. Called from DBD_STATE_UPDATE_GRAPHS
   * and from the constructor after loading the NVS cache.
   */
  void send_to_display_handler();
};
