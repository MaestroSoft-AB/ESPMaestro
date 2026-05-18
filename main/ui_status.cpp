#include "ui_status.hpp"
#include "display_handler.h"
#include "esp_log.h"
#include "ntp_clock.hpp"
#include <string.h>

static const char *TAG = "ui status";
static void ui_status_taskwork(void *_context, uint64_t _montime);

/***********************Taskwork**************************/

UIStatusState ui_status_init() {
  NtpClock::init();
  return UI_STATUS_WAIT_NTP;
}

UIStatusState ui_status_wait_ntp(UiStatus *self, uint64_t now_ms) {
  if (NtpClock::time_valid()) {
    self->sync_time_from_ntp(NtpClock::epoch(), now_ms);
    return UI_STATUS_IDLE;
  }
  return UI_STATUS_WAIT_NTP;
}

UIStatusState ui_status_update_clock(UiStatus *self, uint64_t now_ms) {
  self->update_clock(now_ms);
  display_handler_update_time(self->get_hour(), self->get_minute(),
                              self->get_second());

  return UI_STATUS_IDLE;
};

static void ui_status_taskwork(void *_context, uint64_t _now) {
  UiStatus *self = static_cast<UiStatus *>(_context);
  if (self == nullptr)
    return;

  switch (self->get_state()) {
  case UI_STATUS_INIT:
    ESP_LOGI(TAG, "UI_STATUS_INIT");
    self->set_state(ui_status_init());
    break;

  case UI_STATUS_WAIT_NTP:
    ESP_LOGI(TAG, "UI_STATUS_WAIT_NTP");
    self->set_state(ui_status_wait_ntp(self, _now));
    break;

  case UI_STATUS_IDLE: {
    if (_now >= self->get_next_clock_ms()) {
      self->set_state(UI_STATUS_UPDATE_CLOCK);
      break;
    }
    break;
  }

  case UI_STATUS_UPDATE_CLOCK:
    self->set_state(ui_status_update_clock(self, _now));
    break;

  default:
    break;
  }
}

/*********************Class Methods***********************************/
void UiStatus::sync_time_from_ntp(uint32_t epoch, uint64_t now_ms) {
  base_epoch_ = epoch;
  base_ms_ = now_ms;
  time_valid_ = true;
}

UiStatus::UiStatus()
    : state_(UI_STATUS_INIT), base_epoch_(0), base_ms_(0), next_clock_ms_(0),
      task_(nullptr), time_valid_(true), initialized_(true), hour(0), minute(0),
      second(0) {
  task_ = scheduler_create_task(this, ui_status_taskwork);
  if (task_ == nullptr) {
    initialized_ = false;
  }
}

void UiStatus::update_clock(uint64_t now_ms) {
  if (!time_valid_ || now_ms < next_clock_ms_)
    return;

  uint32_t elapsed = (now_ms - base_ms_) / 1000;
  uint32_t current = base_epoch_ + elapsed;

  uint32_t day = current % 86400;

  uint8_t h = day / 3600;
  uint8_t m = (day / 60) % 60;
  uint8_t s = day % 60;

  set_hour(h);
  set_minute(m);
  set_second(s);

  next_clock_ms_ = now_ms + 1000;
}

UiStatus::~UiStatus() {
  if (initialized_) {
    scheduler_destroy_task(task_);
  }
}
