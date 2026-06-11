#include "bme280_sensor.hpp"

extern "C" {
#include "unity.h"
#include "unity_test_runner.h"
}

TEST_CASE("make_reading converts Bosch data correctly", "[bme280]") {
  bme280_data raw = {};

  raw.temperature = 21.5f;
  raw.humidity = 45.0f;
  raw.pressure = 101325.0f;

  bme280_reading reading = bme280::make_reading(raw, 123456);

  TEST_ASSERT_TRUE(reading.valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.5f, reading.temperature_c);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 45.0f, reading.humidity_rh);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1013.25f, reading.pressure_hpa);
  TEST_ASSERT_EQUAL_UINT32(123456, reading.updated_epoch);
}

TEST_CASE("make_reading preserves raw temp and humidity values", "[bme280]") {
  bme280_data raw = {};

  raw.temperature = -3.25f;
  raw.humidity = 40.32f;
  raw.pressure = 101123.1f;

  bme280_reading reading = bme280::make_reading(raw, 42);

  TEST_ASSERT_TRUE(reading.valid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -3.25f, reading.temperature_c);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.32f, reading.humidity_rh);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1011.231f, reading.pressure_hpa);
  TEST_ASSERT_EQUAL_UINT32(42, reading.updated_epoch);
}

TEST_CASE("make_reading handles zero values", "[bme280]") {
  bme280_data raw = {};

  bme280_reading reading = bme280::make_reading(raw, 0);

  TEST_ASSERT_TRUE(reading.valid);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, reading.temperature_c);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, reading.humidity_rh);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, reading.pressure_hpa);
  TEST_ASSERT_EQUAL_UINT32(0, reading.updated_epoch);
}
