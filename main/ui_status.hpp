#ifndef __UI_STATUS_HPP__
#define __UI_STATUS_HPP__
#include "scheduler.h"
#include <stdbool.h>
#include <stdio.h>

typedef enum {
  UI_STATUS_INIT,
  UI_STATUS_WAIT_NTP,
  UI_STATUS_IDLE,
  UI_STATUS_UPDATE_CLOCK,
} UIStatusState;

class UiStatus {
private:
  UIStatusState state_;
  uint64_t base_epoch_;
  uint64_t base_ms_;
  uint64_t next_clock_ms_;
  Scheduler_Task *task_;
  bool time_valid_;
  bool initialized_;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;

public:
  uint16_t year;
  uint8_t month;
  uint8_t day;

  UiStatus();

  // Prohibit copy
  UiStatus(const UiStatus &) = delete;
  UiStatus &operator=(const UiStatus &) = delete;

  // Prohibit move
  UiStatus(UiStatus &&) = delete;
  UiStatus &operator=(UiStatus &&) = delete;

  UIStatusState get_state() const { return state_; }

  uint64_t get_next_clock_ms() const { return next_clock_ms_; }

  void set_state(UIStatusState state) { state_ = state; }

  void set_hour(uint8_t h) { hour = h; }

  void set_minute(uint8_t m) { minute = m; }

  void set_second(uint8_t s) { second = s; }

  uint8_t get_hour() { return hour; }

  uint8_t get_minute() { return minute; }

  uint8_t get_second() { return second; }

  void set_next_clock_ms(uint64_t ms) { next_clock_ms_ = ms; }

  void sync_time_from_ntp(uint32_t epoch, uint64_t now_ms);

  void update_clock(uint64_t now_ms);

  ~UiStatus();
};

#endif
