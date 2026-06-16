#ifndef __UI_STATUS_HPP__
#define __UI_STATUS_HPP__
#include "bme280_sensor.hpp"
#include "scheduler.h"
#include <stdbool.h>
#include <stdio.h>

/**
 * @brief State machine states used by the UI status module.
 *
 * The normal progression is:
 * UI_STATUS_INIT → UI_STATUS_WAIT_NTP → UI_STATUS_IDLE
 *
 * From UI_STATUS_IDLE the scheduler task transitions to either
 * UI_STATUS_UPDATE_CLOCK or UI_STATUS_UPDATE_BME280 when their respective
 * deadlines are reached, and returns to UI_STATUS_IDLE immediately after.
 */
typedef enum {
  /**
   * @brief Initial state entered on construction.
   *
   * Sets the Swedish timezone (CET/CEST) and calls NtpClock::init().
   * Transitions unconditionally to UI_STATUS_WAIT_NTP.
   */
  UI_STATUS_INIT,

  /**
   * @brief Polls NtpClock::time_valid() until NTP time is available.
   *
   * Once valid, calls UiStatus::sync_time_from_ntp() to record the epoch
   * base and transitions to UI_STATUS_IDLE.
   */
  UI_STATUS_WAIT_NTP,

  /**
   * @brief Idle state. Checks scheduled deadlines each scheduler tick.
   *
   * Transitions to UI_STATUS_UPDATE_CLOCK when the next clock deadline is
   * reached, or to UI_STATUS_UPDATE_BME280 when the BME280 deadline is
   * reached (clock takes priority).
   */
  UI_STATUS_IDLE,

  /**
   * @brief Computes current local time and forwards it to the display handler.
   *
   * Calls UiStatus::update_clock(), which also pushes a date update when the
   * date changes. Transitions back to UI_STATUS_IDLE.
   */
  UI_STATUS_UPDATE_CLOCK,

  /**
   * @brief Reads the latest BME280 reading and forwards it to the display
   * handler.
   *
   * Skipped (returns to UI_STATUS_IDLE immediately) if no sensor is attached.
   * Schedules the next BME280 update 5000 ms in the future. Transitions back
   * to UI_STATUS_IDLE.
   */
  UI_STATUS_UPDATE_BME280
} UIStatusState;

/**
 * @brief UI status controller for clock, date, and BME280 display updates.
 *
 * Owns a scheduler task that drives a small state machine. The state machine:
 * - Configures the Swedish timezone (CET/CEST via the POSIX TZ string) and
 *   starts NTP synchronization on first run.
 * - Waits for a valid NTP timestamp, then derives local time from an
 *   (epoch, monotonic-ms) base pair using localtime_r(), avoiding repeated
 *   calls to time().
 * - Updates the display clock once per second via
 *   display_handler_update_time() and pushes a date update via
 *   display_handler_update_date() only when the date changes.
 * - Forwards the latest BME280 reading to the display handler every 5 seconds
 *   when a sensor is attached.
 *
 * The class does not own the BME280 sensor; the pointer is optional and must
 * remain valid for the lifetime of this object if supplied.
 *
 * Copying and moving are disabled because the object owns a scheduler task.
 */
class UiStatus {
private:
  /** @brief Current state of the UI status state machine. */
  UIStatusState state_;

  /**
   * @brief Unix epoch timestamp recorded at the last NTP synchronization.
   *
   * Combined with @p base_ms_ to derive the current time without calling
   * time() on every tick.
   */
  uint64_t base_epoch_;

  /**
   * @brief Monotonic millisecond timestamp corresponding to @p base_epoch_.
   *
   * Set by sync_time_from_ntp() at the moment the NTP epoch is received.
   */
  uint64_t base_ms_;

  /**
   * @brief Monotonic millisecond timestamp after which the next clock update
   * is due.
   *
   * Set to now + 1000 ms at the end of each update_clock() call.
   */
  uint64_t next_clock_ms_;

  /** @brief Scheduler task that drives the state machine. */
  Scheduler_Task *task_;

  /** @brief True after a valid NTP timestamp has been received and stored. */
  bool time_valid_;

  /**
   * @brief True if the constructor succeeded in creating the scheduler task.
   *
   * Checked by the destructor to decide whether to call
   * scheduler_destroy_task().
   */
  bool initialized_;

  /**
   * @brief The last state that was passed to should_log_state().
   *
   * Used together with @p has_logged_state_ to suppress duplicate log lines
   * for the same state.
   */
  UIStatusState logged_state_;

  /** @brief True after should_log_state() has been called at least once. */
  bool has_logged_state_;

  /** @brief Cached local hour derived from the NTP base (0–23). */
  uint8_t hour;

  /** @brief Cached local minute derived from the NTP base (0–59). */
  uint8_t minute;

  /** @brief Cached local second derived from the NTP base (0–59). */
  uint8_t second;

public:
  /** @brief Current local year (four-digit, e.g. 2025). */
  uint16_t year;

  /** @brief Current local month in the range 1–12. */
  uint8_t month;

  /** @brief Current local day-of-month in the range 1–31. */
  uint8_t day;

  /**
   * @brief Optional BME280 sensor used for environmental display updates.
   *
   * May be null. When non-null the object must remain valid for the lifetime
   * of this UiStatus instance. Checked in UI_STATUS_UPDATE_BME280.
   */
  bme280 *sensor;

  /**
   * @brief Monotonic millisecond timestamp after which the next BME280 display
   * update is due.
   *
   * Set to now + 5000 ms at the end of each UI_STATUS_UPDATE_BME280 handling.
   * Initialized to 0 so the first update fires immediately after NTP sync.
   */
  uint64_t next_bme280_ms;

  /**
   * @brief Construct the UI status controller and start the scheduler task.
   *
   * Initializes all members and creates a scheduler task that will run
   * ui_status_taskwork() on every scheduler tick. The state machine starts in
   * UI_STATUS_INIT. If task creation fails, @p initialized_ is set to false
   * and no scheduler task is registered.
   *
   * @param sensor Optional BME280 sensor pointer. The sensor is not owned by
   *               this object and must outlive it if non-null. Pass nullptr
   *               (default) to disable environmental updates.
   */
  UiStatus(class bme280 *sensor = nullptr);

  /** @brief Copy construction disabled — object owns a scheduler task. */
  UiStatus(const UiStatus &) = delete;

  /** @brief Copy assignment disabled — object owns a scheduler task. */
  UiStatus &operator=(const UiStatus &) = delete;

  /** @brief Move construction disabled — object owns a scheduler task. */
  UiStatus(UiStatus &&) = delete;

  /** @brief Move assignment disabled — object owns a scheduler task. */
  UiStatus &operator=(UiStatus &&) = delete;

  /**
   * @brief Destroy the UI status controller.
   *
   * Calls scheduler_destroy_task() if the scheduler task was successfully
   * created during construction.
   */
  ~UiStatus();

  /**
   * @brief Get the current state machine state.
   *
   * @return Current @ref UIStatusState.
   */
  UIStatusState get_state() const { return state_; }

  /**
   * @brief Set the current state machine state.
   *
   * Intended to be called from the scheduler task function, not from external
   * code.
   *
   * @param state New state.
   */
  void set_state(UIStatusState state) { state_ = state; }

  /**
   * @brief Get the monotonic deadline for the next clock update.
   *
   * @return Monotonic timestamp in milliseconds.
   */
  uint64_t get_next_clock_ms() const { return next_clock_ms_; }

  /**
   * @brief Override the monotonic deadline for the next clock update.
   *
   * @param ms New deadline in monotonic milliseconds.
   */
  void set_next_clock_ms(uint64_t ms) { next_clock_ms_ = ms; }

  /** @brief Set the cached local hour (0–23). @param h Hour value. */
  void set_hour(uint8_t h) { hour = h; }

  /** @brief Set the cached local minute (0–59). @param m Minute value. */
  void set_minute(uint8_t m) { minute = m; }

  /** @brief Set the cached local second (0–59). @param s Second value. */
  void set_second(uint8_t s) { second = s; }

  /** @brief Get the cached local hour (0–23). @return Hour value. */
  uint8_t get_hour() { return hour; }

  /** @brief Get the cached local minute (0–59). @return Minute value. */
  uint8_t get_minute() { return minute; }

  /** @brief Get the cached local second (0–59). @return Second value. */
  uint8_t get_second() { return second; }

  /**
   * @brief Record an NTP-synchronized time base.
   *
   * Stores @p epoch and @p now_ms as the reference pair used by update_clock()
   * to derive local time on subsequent ticks without calling time(). Also sets
   * @p time_valid_ to true, which allows the state machine to leave
   * UI_STATUS_WAIT_NTP.
   *
   * @param epoch  Unix epoch timestamp received from NtpClock::epoch().
   * @param now_ms Monotonic time in milliseconds at the moment @p epoch was
   *               read (from the scheduler tick timestamp).
   */
  void sync_time_from_ntp(uint32_t epoch, uint64_t now_ms);

  /**
   * @brief Compute current local time and push display updates.
   *
   * Derives the current time by adding the elapsed milliseconds since
   * @p base_ms_ to @p base_epoch_, then calls localtime_r() to decompose
   * the result into local time using the configured timezone. Caches hour,
   * minute, second, year, month, and day. Calls
   * display_handler_update_date() only when the date has changed since the
   * last call. Advances @p next_clock_ms_ by 1000 ms.
   *
   * Does nothing if @p time_valid_ is false or @p now_ms is before
   * @p next_clock_ms_.
   *
   * @param now_ms Current monotonic time in milliseconds.
   */
  void update_clock(uint64_t now_ms);

  /**
   * @brief Suppress duplicate state log lines.
   *
   * Returns true and records @p state the first time it is seen, or whenever
   * it differs from the last recorded state. Returns false if @p state equals
   * the previously recorded state, indicating the log line should be skipped.
   *
   * Clock and BME280 update states are excluded from logging at the call site
   * to avoid per-second noise.
   *
   * @param state State to evaluate.
   * @return true  if the state should be logged.
   * @return false if the state was already logged and has not changed.
   */
  bool should_log_state(UIStatusState state);
};

#endif
