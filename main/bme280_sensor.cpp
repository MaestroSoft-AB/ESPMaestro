#include "bme280_sensor.hpp"
#include "display_handler.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "time.h"
#include <string.h>

#define BME280_RETRY_MS 3000
#define BME280_TASK_STACK 4096
#define BME280_TASK_PRIORITY 2

static const char *TAG = "bme280";

/*------------Cache helpers----------------*/
static void bme_cache_save_float(const char *key, float value) {
  nvs_handle_t h;
  if (nvs_open("bme_cache", NVS_READWRITE, &h) != ESP_OK)
    return;

  int32_t scaled = (int32_t)(value * 100.0f);
  nvs_set_i32(h, key, scaled);
  nvs_commit(h);
  nvs_close(h);
}

static bool bme_cache_load_float(const char *key, float *out) {
  if (!out)
    return false;

  nvs_handle_t h;
  if (nvs_open("bme_cache", NVS_READONLY, &h) != ESP_OK)
    return false;

  int32_t scaled = 0;
  esp_err_t err = nvs_get_i32(h, key, &scaled);
  nvs_close(h);

  if (err != ESP_OK)
    return false;

  *out = ((float)scaled) / 100.0f;
  return true;
}

static void bme_cache_save_u32(const char *key, uint32_t value) {
  nvs_handle_t h;
  if (nvs_open("bme_cache", NVS_READWRITE, &h) != ESP_OK)
    return;

  nvs_set_u32(h, key, value);
  nvs_commit(h);
  nvs_close(h);
}

static bool bme_cache_load_u32(const char *key, uint32_t *out) {
  if (!out)
    return false;

  nvs_handle_t h;
  if (nvs_open("bme_cache", NVS_READONLY, &h) != ESP_OK)
    return false;

  esp_err_t err = nvs_get_u32(h, key, out);
  nvs_close(h);

  return err == ESP_OK;
}

/*-------------------------------------------------------------------*/

bool bme280::init(i2c_master_bus_handle_t bus) {
  if (initialized_)
    return true;

  if (init_at_address(bus, 0x77)) {
    ESP_LOGI(TAG, "bme280 initialized at 0x77");
    return true;
  }

  if (init_at_address(bus, 0x76)) {
    ESP_LOGI(TAG, "bme280 initialized at 0x76");
    return true;
  }

  ESP_LOGW(TAG, "bme280 not found");
  return false;
}

bool bme280::init_at_address(i2c_master_bus_handle_t bus, uint8_t address) {
  if (bus == nullptr) {
    ESP_LOGE(TAG, "I2C bus is null");
    return false;
  }

  i2c_device_config_t dev_config = {};
  dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  dev_config.device_address = address;
  dev_config.scl_speed_hz = 100000;

  i2c_master_dev_handle_t temp_dev = nullptr;
  esp_err_t err = i2c_master_bus_add_device(bus, &dev_config, &temp_dev);

  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to add I2C device 0x%02X: %s", address,
             esp_err_to_name(err));
    return false;
  }

  memset(&bosch_dev_, 0, sizeof(bosch_dev_));

  dev_handle_ = temp_dev;
  address_ = address;

  i2c_ctx_.dev = dev_handle_;
  i2c_ctx_.mutex = i2c_mutex_;

  bosch_dev_.intf = BME280_I2C_INTF;
  bosch_dev_.read = bme280::i2c_read;
  bosch_dev_.write = bme280::i2c_write;
  bosch_dev_.delay_us = bme280::delay_us;
  bosch_dev_.intf_ptr = &i2c_ctx_;

  int8_t res = bme280_init(&bosch_dev_);

  if (res != BME280_OK) {
    ESP_LOGW(TAG, "Bosch bme280_init failed at 0x%02X: %d", address, res);
    i2c_master_bus_rm_device(temp_dev);
    dev_handle_ = nullptr;
    i2c_ctx_.dev = nullptr;
    address_ = 0;
    return false;
  }

  struct bme280_settings settings = {};
  settings.osr_h = BME280_OVERSAMPLING_1X;
  settings.osr_p = BME280_OVERSAMPLING_16X;
  settings.osr_t = BME280_OVERSAMPLING_2X;
  settings.filter = BME280_FILTER_COEFF_16;
  settings.standby_time = BME280_STANDBY_TIME_1000_MS;

  uint8_t settings_sel = BME280_SEL_OSR_HUM | BME280_SEL_OSR_PRESS |
                         BME280_SEL_OSR_TEMP | BME280_SEL_FILTER |
                         BME280_SEL_STANDBY;

  res = bme280_set_sensor_settings(settings_sel, &settings, &bosch_dev_);

  if (res != BME280_OK) {
    ESP_LOGE(TAG, "Failed to configure BME280: %d", res);
    i2c_master_bus_rm_device(temp_dev);
    dev_handle_ = nullptr;
    i2c_ctx_.dev = nullptr;
    address_ = 0;
    return false;
  }

  res = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &bosch_dev_);

  if (res != BME280_OK) {
    ESP_LOGE(TAG, "Failed to set BME280 normal mode: %d", res);
    i2c_master_bus_rm_device(temp_dev);
    dev_handle_ = nullptr;
    i2c_ctx_.dev = nullptr;
    address_ = 0;
    return false;
  }

  initialized_ = true;
  latest_ = {};
  return true;
}

bool bme280::read() {
  if (!initialized_)
    return false;

  struct bme280_data data = {};

  int8_t res = bme280_get_sensor_data(BME280_ALL, &data, &bosch_dev_);

  if (res != BME280_OK) {
    ESP_LOGW(TAG, "Failed to read BME280 data: %d", res);
    return false;
  }
  latest_ = make_reading(data, (uint32_t)time(nullptr));

  display_handler_update_bme280(latest_.temperature_c, latest_.humidity_rh,
                                latest_.pressure_hpa);

  bme_cache_save_float("temp_c", latest_.temperature_c);
  bme_cache_save_float("humidity", latest_.humidity_rh);
  bme_cache_save_float("hpa", latest_.pressure_hpa);
  bme_cache_save_u32("epoch", latest_.updated_epoch);

  return true;
}

bool bme280::latest(bme280_reading *out) const {
  if (out == nullptr || !latest_.valid)
    return false;

  *out = latest_;
  return true;
}

BME280_INTF_RET_TYPE bme280::i2c_read(uint8_t reg_addr, uint8_t *reg_data,
                                      uint32_t len, void *intf_ptr) {
  if (intf_ptr == nullptr || reg_data == nullptr)
    return BME280_E_NULL_PTR;

  auto ctx = static_cast<bme280_i2c_context *>(intf_ptr);

  if (ctx->dev == nullptr || ctx->mutex == nullptr)
    return BME280_E_COMM_FAIL;

  if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    return BME280_E_COMM_FAIL;

  esp_err_t err = i2c_master_transmit_receive(ctx->dev, &reg_addr, 1, reg_data,
                                              len, pdMS_TO_TICKS(100));

  xSemaphoreGive(ctx->mutex);

  return err == ESP_OK ? BME280_INTF_RET_SUCCESS : BME280_E_COMM_FAIL;
}

BME280_INTF_RET_TYPE bme280::i2c_write(uint8_t reg_addr,
                                       const uint8_t *reg_data, uint32_t len,
                                       void *intf_ptr) {
  if (intf_ptr == nullptr || reg_data == nullptr)
    return BME280_E_NULL_PTR;

  auto ctx = static_cast<bme280_i2c_context *>(intf_ptr);

  if (ctx->dev == nullptr || ctx->mutex == nullptr)
    return BME280_E_COMM_FAIL;

  uint8_t buffer[32];

  if (len + 1 > sizeof(buffer))
    return BME280_E_COMM_FAIL;

  buffer[0] = reg_addr;
  memcpy(&buffer[1], reg_data, len);

  if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    return BME280_E_COMM_FAIL;

  esp_err_t err =
      i2c_master_transmit(ctx->dev, buffer, len + 1, pdMS_TO_TICKS(100));

  xSemaphoreGive(ctx->mutex);

  return err == ESP_OK ? BME280_INTF_RET_SUCCESS : BME280_E_COMM_FAIL;
}

bme280_reading bme280::make_reading(const struct bme280_data &data,
                                    uint32_t epoch) {
  bme280_reading reading = {};

  reading.valid = true;
  reading.temperature_c = data.temperature;
  reading.humidity_rh = data.humidity;
  reading.pressure_hpa = data.pressure / 100.0f;
  reading.updated_epoch = epoch;

  return reading;
}

void bme280::delay_us(uint32_t period, void *intf_ptr) {
  (void)intf_ptr;
  esp_rom_delay_us(period);
}

bool bme280::start(i2c_master_bus_handle_t bus, SemaphoreHandle_t i2c_mutex,
                   uint32_t interval_ms) {
  if (api_task_ != nullptr) {
    return true;
  }

  bus_ = bus;
  i2c_mutex_ = i2c_mutex;
  api_interval_ms_ = interval_ms;

  float temp = 0.0f;
  float humidity = 0.0f;
  float hpa = 0.0f;
  uint32_t epoch = 0;

  if (bme_cache_load_float("temp_c", &temp) &&
      bme_cache_load_float("humidity", &humidity) &&
      bme_cache_load_float("hpa", &hpa)) {
    bme280_reading cached = {};
    cached.valid = true;
    cached.temperature_c = temp;
    cached.humidity_rh = humidity;
    cached.pressure_hpa = hpa;

    if (bme_cache_load_u32("epoch", &epoch)) {
      cached.updated_epoch = epoch;
    } else {
      cached.updated_epoch = 0;
    }

    latest_ = cached;
    display_handler_update_bme280(latest_.temperature_c, latest_.humidity_rh,
                                  latest_.pressure_hpa);
  }

  return xTaskCreate(api_task_entry, "bme280", BME280_TASK_STACK, this,
                     BME280_TASK_PRIORITY, &api_task_) == pdPASS;
}

void bme280::stop() {
  if (api_task_ != nullptr) {
    vTaskDelete(api_task_);
    api_task_ = nullptr;
  }
}

void bme280::api_task_entry(void *arg) {
  bme280 *self = static_cast<bme280 *>(arg);

  vTaskDelay(pdMS_TO_TICKS(5000));

  while (1) {
    if (!self->initialized_) {
      if (!self->init(self->bus_)) {
        vTaskDelay(pdMS_TO_TICKS(BME280_RETRY_MS));
        continue;
      }
    }

    self->read();
    vTaskDelay(pdMS_TO_TICKS(self->api_interval_ms_));
  }
}
