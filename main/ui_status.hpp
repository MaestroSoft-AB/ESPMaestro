#ifndef __UI_STATUS_HPP__
#define __UI_STATUS_HPP__
#include "bme280_sensor.hpp"
#include "scheduler.h"
#include <stdbool.h>
#include <stdio.h>

/**
 * @brief State machine states used by the UI status module.
 */
typedef enum {
  /** @brief Initial state. Initializes timezone and NTP handling. */
  UI_STATUS_INIT,

  /** @brief Waiting for NTP time synchronization to become valid. */
  UI_STATUS_WAIT_NTP,

  /** @brief Idle state waiting for the next clock or sensor update. */
  UI_STATUS_IDLE,

  /** @brief Updates the displayed clock and date. */
  UI_STATUS_UPDATE_CLOCK,

  /** @brief Updates displayed BME280 temperature, humidity, and pressure data.
   */
  UI_STATUS_UPDATE_BME280
} UIStatusState;

/**
 * @brief UI status controller for clock, date, and BME280 display updates.
 *
 * This class owns a scheduler task that drives a small state machine.
 * The state machine initializes Swedish timezone handling, waits for valid
 * NTP time, updates the display clock once per second, updates the date when it
 * changes, and periodically forwards the latest BME280 reading to the display
 * handler.
 *
 * The class does not own the BME280 sensor instance. The sensor pointer is
 * optional and may be null.
 *
 * Copying and moving are disabled because the class owns a scheduler task.
 */
class UiStatus {
private:
  /** @brief Current state of the UI status state machine. */
  UIStatusState state_;

  /** @brief Reference Unix epoch received from NTP. */
  uint64_t base_epoch_;

  /** @brief Monotonic millisecond timestamp corresponding to base_epoch_. */
  uint64_t base_ms_;

  /** @brief Monotonic millisecond timestamp for the next clock update. */
  uint64_t next_clock_ms_;

  /** @brief Scheduler task used to execute the UI status state machine. */
  Scheduler_Task *task_;

  /** @brief True after a valid NTP time has been synchronized. */
  bool time_valid_;

  /** @brief True if scheduler task initialization succeeded. */
  bool initialized_;

  /** @brief Last state that was logged. */
  UIStatusState logged_state_;

  /** @brief True after at least one state has been logged. */
  bool has_logged_state_;

  /** @brief Current local hour. */
  uint8_t hour;

  /** @brief Current local minute. */
  uint8_t minute;

  /** @brief Current local second. */
  uint8_t second;

public:
  /** @brief Current local year. */
  uint16_t year;

  /** @brief Current local month, in the range 1-12. */
  uint8_t month;

  /** @brief Current local day of month, in the range 1-31. */
  uint8_t day;

  /** @brief Optional BME280 sensor used for environmental display updates. */
  bme280 *sensor;

  /** @brief Monotonic millisecond timestamp for the next BME280 display update.
   */
  uint64_t next_bme280_ms;

  /**
   * @brief Construct the UI status controller.
   *
   * Creates a scheduler task for the UI status state machine. The state machine
   * starts in UI_STATUS_INIT and will later wait for NTP synchronization before
   * updating the clock.
   *
   * @param sensor Optional pointer to a BME280 sensor wrapper. The object is
   * not owned by UiStatus and must remain valid while it is used.
   */
  UiStatus(class bme280 *sensor = nullptr);

  /** @brief Copy construction is disabled because this object owns scheduler
   * resources. */
  UiStatus(const UiStatus &) = delete;

  /** @brief Copy assignment is disabled because this object owns scheduler
   * resources. */
  UiStatus &operator=(const UiStatus &) = delete;

  /** @brief Move construction is disabled because this object owns scheduler
   * resources. */
  UiStatus(UiStatus &&) = delete;

  /** @brief Move assignment is disabled because this object owns scheduler
   * resources. */
  UiStatus &operator=(UiStatus &&) = delete;

  /**
   * @brief Get the current UI status state.
   *
   * @return Current state machine state.
   */
  UIStatusState get_state() const { return state_; }

  /**
   * @brief Get the monotonic timestamp for the next clock update.
   *
   * @return Next scheduled clock update time in milliseconds.
   */
  uint64_t get_next_clock_ms() const { return next_clock_ms_; }

  /**
   * @brief Set the current UI status state.
   *
   * @param state New state machine state.
   */
  void set_state(UIStatusState state) { state_ = state; }

  /**
   * @brief Set the cached local hour.
   *
   * @param h Hour value.
   */
  void set_hour(uint8_t h) { hour = h; }

  /**
   * @brief Set the cached local minute.
   *
   * @param m Minute value.
   */
  void set_minute(uint8_t m) { minute = m; }

  /**
   * @brief Set the cached local second.
   *
   * @param s Second value.
   */
  void set_second(uint8_t s) { second = s; }

  /**
   * @brief Get the cached local hour.
   *
   * @return Hour value.
   */
  uint8_t get_hour() { return hour; }

  /**
   * @brief Get the cached local minute.
   *
   * @return Minute value.
   */
  uint8_t get_minute() { return minute; }

  /**
   * @brief Get the cached local second.
   *
   * @return Second value.
   */
  uint8_t get_second() { return second; }

  /**
   * @brief Set the monotonic timestamp for the next clock update.
   *
   * @param ms Next clock update time in milliseconds.
   */
  void set_next_clock_ms(uint64_t ms) { next_clock_ms_ = ms; }

  /**
   * @brief Synchronize the local clock base from NTP.
   *
   * Stores the received Unix epoch together with the current monotonic
   * millisecond timestamp. Later clock updates derive local time from this
   * base pair.
   *
   * @param epoch Unix epoch timestamp received from NTP.
   * @param now_ms Current monotonic time in milliseconds.
   */
  void sync_time_from_ntp(uint32_t epoch, uint64_t now_ms);

  /**
   * @brief Update cached local time and refresh the display if needed.
   *
   * Computes the current local time from the NTP base epoch and elapsed
   * monotonic time. The display date is updated only when the date changes.
   * The next clock update is scheduled one second later.
   *
   * @param now_ms Current monotonic time in milliseconds.
   */
  void update_clock(uint64_t now_ms);

  /**
   * @brief Check whether a state transition should be logged.
   *
   * Prevents repeated log output for the same state.
   *
   * @param state State to check.
   * @return true if the state should be logged, false if it was already logged.
   */
  bool should_log_state(UIStatusState state);

  /**
   * @brief Destroy the UI status controller.
   *
   * Destroys the owned scheduler task if initialization succeeded.
   */
  ~UiStatus();
};

#endif
