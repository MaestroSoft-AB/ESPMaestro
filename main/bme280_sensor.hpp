#pragma once

#include "../components/bme280_driver/include/bme280.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

typedef struct {
  bool valid;
  float temperature_c;
  float humidity_rh;
  float pressure_hpa;
  uint32_t updated_epoch;
} bme280_reading;

class bme280 {
public:
  bme280() = default;

  bool init(i2c_master_bus_handle_t bus);
  bool read();
  bool latest(bme280_reading *out) const;
  bool start_api_task(uint32_t interval_ms = 2000);
  void stop_api_task();

private:
  bool initialized_ = false;
  uint8_t address_ = 0;
  i2c_master_dev_handle_t dev_handle_ = nullptr;

  struct bme280_dev bosch_dev_ = {};
  bme280_reading latest_ = {};

  TaskHandle_t api_task_ = nullptr;
  uint32_t api_interval_ms_ = 2000;

  bool init_at_address(i2c_master_bus_handle_t bus, uint8_t address);

  static BME280_INTF_RET_TYPE i2c_read(uint8_t reg_addr, uint8_t *reg_data,
                                       uint32_t len, void *intf_ptr);
  static BME280_INTF_RET_TYPE i2c_write(uint8_t reg_addr,
                                        const uint8_t *reg_data, uint32_t len,
                                        void *intf_ptr);

  static void delay_us(uint32_t period, void *intf_ptr);

  static void api_task_entry(void *arg);
};
