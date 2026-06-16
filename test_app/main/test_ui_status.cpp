#include "ui_status.hpp"

#include <stdlib.h>

extern "C" {
#include "unity.h"
#include "unity_test_runner.h"
}

UIStatusState ui_status_update_clock(UiStatus *self, uint64_t now_ms);
UIStatusState ui_status_update_bme280(UiStatus *self, uint64_t now_ms);

extern "C" {
static uint8_t s_display_hour;
static uint8_t s_display_minute;
static uint8_t s_display_second;
static uint16_t s_display_year;
static uint8_t s_display_month;
static uint8_t s_display_day;
static int s_time_update_count;
static int s_date_update_count;
static int s_bme280_update_count;

void display_handler_update_time(uint8_t h, uint8_t m, uint8_t s) {
  s_display_hour = h;
  s_display_minute = m;
  s_display_second = s;
  s_time_update_count++;
}

void display_handler_update_date(uint16_t year, uint8_t month, uint8_t day) {
  s_display_year = year;
  s_display_month = month;
  s_display_day = day;
  s_date_update_count++;
}

void display_handler_update_bme280(float temp, float humidity, float hpa) {
  (void)temp;
  (void)humidity;
  (void)hpa;
  s_bme280_update_count++;
}
}

static void reset_display_spy() {
  s_display_hour = 0;
  s_display_minute = 0;
  s_display_second = 0;
  s_display_year = 0;
  s_display_month = 0;
  s_display_day = 0;
  s_time_update_count = 0;
  s_date_update_count = 0;
  s_bme280_update_count = 0;
}

TEST_CASE("UiStatus starts in init state and registers scheduler task",
          "[ui_status]") {
  scheduler_init();

  {
    UiStatus status;

    TEST_ASSERT_EQUAL(UI_STATUS_INIT, status.get_state());
    TEST_ASSERT_EQUAL_UINT64(0, status.get_next_clock_ms());
    TEST_ASSERT_EQUAL_UINT64(0, status.next_bme280_ms);
    TEST_ASSERT_NULL(status.sensor);
    TEST_ASSERT_EQUAL_INT(1, scheduler_get_task_count());
  }

  TEST_ASSERT_EQUAL_INT(0, scheduler_get_task_count());
}

TEST_CASE("UiStatus should_log_state suppresses duplicate state logs",
          "[ui_status]") {
  scheduler_init();
  UiStatus status;

  TEST_ASSERT_TRUE(status.should_log_state(UI_STATUS_INIT));
  TEST_ASSERT_FALSE(status.should_log_state(UI_STATUS_INIT));
  TEST_ASSERT_TRUE(status.should_log_state(UI_STATUS_WAIT_NTP));
  TEST_ASSERT_FALSE(status.should_log_state(UI_STATUS_WAIT_NTP));
}

TEST_CASE("UiStatus update_clock uses synchronized epoch and advances deadline",
          "[ui_status]") {
  scheduler_init();
  reset_display_spy();
  setenv("TZ", "UTC0", 1);
  tzset();

  UiStatus status;
  status.year = 0;
  status.month = 0;
  status.day = 0;

  status.sync_time_from_ntp(1704067200, 1000);
  status.update_clock(1000);

  TEST_ASSERT_EQUAL_UINT8(0, status.get_hour());
  TEST_ASSERT_EQUAL_UINT8(0, status.get_minute());
  TEST_ASSERT_EQUAL_UINT8(0, status.get_second());
  TEST_ASSERT_EQUAL_UINT16(2024, status.year);
  TEST_ASSERT_EQUAL_UINT8(1, status.month);
  TEST_ASSERT_EQUAL_UINT8(1, status.day);
  TEST_ASSERT_EQUAL_UINT64(2000, status.get_next_clock_ms());
  TEST_ASSERT_EQUAL_INT(1, s_date_update_count);
  TEST_ASSERT_EQUAL_UINT16(2024, s_display_year);
  TEST_ASSERT_EQUAL_UINT8(1, s_display_month);
  TEST_ASSERT_EQUAL_UINT8(1, s_display_day);

  status.update_clock(1999);
  TEST_ASSERT_EQUAL_UINT8(0, status.get_second());
  TEST_ASSERT_EQUAL_UINT64(2000, status.get_next_clock_ms());

  status.update_clock(2500);
  TEST_ASSERT_EQUAL_UINT8(1, status.get_second());
  TEST_ASSERT_EQUAL_UINT64(3500, status.get_next_clock_ms());
}

TEST_CASE("ui_status_update_clock forwards cached time to display handler",
          "[ui_status]") {
  scheduler_init();
  reset_display_spy();
  setenv("TZ", "UTC0", 1);
  tzset();

  UiStatus status;
  status.sync_time_from_ntp(1704071105, 5000);

  TEST_ASSERT_EQUAL(UI_STATUS_IDLE, ui_status_update_clock(&status, 5000));
  TEST_ASSERT_EQUAL_INT(1, s_time_update_count);
  TEST_ASSERT_EQUAL_UINT8(1, s_display_hour);
  TEST_ASSERT_EQUAL_UINT8(5, s_display_minute);
  TEST_ASSERT_EQUAL_UINT8(5, s_display_second);
}

TEST_CASE("ui_status_update_bme280 skips display update when sensor is absent",
          "[ui_status]") {
  scheduler_init();
  reset_display_spy();

  UiStatus status(nullptr);

  TEST_ASSERT_EQUAL(UI_STATUS_IDLE, ui_status_update_bme280(&status, 1234));
  TEST_ASSERT_EQUAL_INT(0, s_bme280_update_count);
  TEST_ASSERT_EQUAL_UINT64(0, status.next_bme280_ms);
}
