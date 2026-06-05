// bme280_probe.hpp
#pragma once

#include "driver/i2c_master.h"
#include "esp_log.h"

class bme280 {
public:
  bool init() {
    i2c_master_bus_config_t bus_config = {};
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = GPIO_NUM_8;
    bus_config.scl_io_num = GPIO_NUM_9;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_config, &bus_);

    if (err == ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "I2C bus already initialized elsewhere");
      return false;
    }

    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(err));
      return false;
    }

    return true;
  }

  bool checkDevice(uint8_t address) {
    if (!bus_) {
      return false;
    }

    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = address;
    dev_config.scl_speed_hz = 100000;

    i2c_master_dev_handle_t dev = nullptr;

    esp_err_t err = i2c_master_bus_add_device(bus_, &dev_config, &dev);

    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to add device 0x%02X: %s", address,
               esp_err_to_name(err));
      return false;
    }

    uint8_t reg = 0xD0;
    uint8_t chip_id = 0;

    err = i2c_master_transmit_receive(dev, &reg, 1, &chip_id, 1, 100);

    i2c_master_bus_rm_device(dev);

    if (err != ESP_OK) {
      ESP_LOGW(TAG, "No response at 0x%02X: %s", address, esp_err_to_name(err));
      return false;
    }

    ESP_LOGI(TAG, "Device at 0x%02X chip id: 0x%02X", address, chip_id);

    return chip_id == 0x60;
  }

private:
  static constexpr const char *TAG = "BME280";
  i2c_master_bus_handle_t bus_ = nullptr;
};
