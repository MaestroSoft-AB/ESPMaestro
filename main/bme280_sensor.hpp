#pragma once

#include "../components/bme280_driver/include/bme280.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

struct bme280_i2c_context {
  i2c_master_dev_handle_t dev;
  SemaphoreHandle_t mutex;
};

/**
 * @brief Cached environmental reading from the BME280 sensor.
 */
typedef struct {
  /** @brief True when this structure contains a valid sensor reading. */
  bool valid;

  /** @brief Temperature in degrees Celsius. */
  float temperature_c;

  /** @brief Relative humidity in percent RH. */
  float humidity_rh;

  /** @brief Atmospheric pressure in hectopascals. */
  float pressure_hpa;

  /** @brief Unix epoch timestamp from time(nullptr) when the reading was
   * updated. */
  uint32_t updated_epoch;
} bme280_reading;

/**
 * @brief ESP-IDF/FreeRTOS wrapper for a Bosch BME280 sensor over I2C.
 *
 * The class probes the standard BME280 I2C addresses, configures the Bosch
 * BME280 driver in normal mode, stores the latest measurement, and can run a
 * FreeRTOS task that periodically updates the cached reading.
 */
class bme280 {
public:
  /**
   * @brief Construct an uninitialized BME280 sensor wrapper.
   */
  bme280() = default;

  /**
   * @brief Initialize the BME280 sensor on an I2C master bus.
   *
   * If already initialized, this function returns true immediately. Otherwise,
   * it probes address 0x77 first and then 0x76. On success, the sensor is
   * configured with humidity oversampling 1x, pressure oversampling 16x,
   * temperature oversampling 2x, filter coefficient 16, 1000 ms standby time,
   * and normal power mode.
   *
   * @param bus ESP-IDF I2C master bus handle.
   * @return true if the sensor is already initialized or was initialized
   * successfully, false if no sensor could be initialized.
   */
  bool init(i2c_master_bus_handle_t bus);

  /**
   * @brief Read one measurement from the initialized sensor.
   *
   * Reads temperature, humidity, and pressure using the Bosch BME280 driver.
   * On success, the cached latest reading is updated and marked valid. Pressure
   * is converted from pascals to hectopascals.
   *
   * @return true if the measurement was read successfully, false if the sensor
   * is not initialized or the Bosch driver read failed.
   */
  bool read();

  /**
   * @brief Copy the latest valid cached reading.
   *
   * @param out Destination pointer that receives the cached reading.
   * @return true if @p out is not null and a valid cached reading exists,
   * false otherwise.
   */
  bool latest(bme280_reading *out) const;

  /**
   * @brief Convert raw Bosch BME280 data to cached application reading.
   *
   * Converts pressure from pascals to hectopascals and marks the reading valid.
   *
   * @param data Raw Bosch sensor data.
   * @param epoch Unix epoch timestamp to store in the reading.
   * @return Converted application reading.
   */
  static bme280_reading make_reading(const struct bme280_data &data,
                                     uint32_t epoch);

  /**
   * @brief Start a background FreeRTOS task for periodic readings.
   *
   * The task initializes the sensor if needed. If initialization fails, it
   * retries every BME280_RETRY_MS milliseconds. Once initialized, it calls
   * read() every @p interval_ms milliseconds.
   *
   * Calling this function while the task is already running returns true
   * without creating another task.
   *
   * @param bus ESP-IDF I2C master bus handle used by the background task.
   * @param interval_ms Delay between successful periodic read attempts.
   * @return true if the task is already running or was created successfully,
   * false if task creation failed.
   */
  bool start(i2c_master_bus_handle_t bus, SemaphoreHandle_t i2c_mutex,
             uint32_t interval_ms = 2000);

  /**
   * @brief Stop the background FreeRTOS task if it is running.
   *
   * Deletes the task and clears the internal task handle.
   */
  void stop();

private:
  /** @brief True after successful Bosch driver initialization and
   * configuration. */
  bool initialized_ = false;

  /** @brief Active 7-bit I2C address of the BME280 sensor. */
  uint8_t address_ = 0;

  /** @brief I2C master bus handle used by the background task. */
  i2c_master_bus_handle_t bus_ = nullptr;

  /** @brief I2C device handle registered for the active BME280 address. */
  i2c_master_dev_handle_t dev_handle_ = nullptr;

  SemaphoreHandle_t i2c_mutex_ = nullptr;
  bme280_i2c_context i2c_ctx_ = {};

  /** @brief Bosch BME280 driver context and callback configuration. */
  struct bme280_dev bosch_dev_ = {};

  /** @brief Last successfully cached sensor reading. */
  bme280_reading latest_ = {};

  /** @brief FreeRTOS task handle for periodic BME280 updates. */
  TaskHandle_t api_task_ = nullptr;

  /** @brief Periodic read interval used by the background task, in
   * milliseconds. */
  uint32_t api_interval_ms_ = 2000;

  /**
   * @brief Try to initialize and configure the sensor at a specific I2C
   * address.
   *
   * Adds an ESP-IDF I2C device to the bus, sets up the Bosch BME280 driver
   * callbacks, verifies the sensor with bme280_init(), applies sensor settings,
   * and switches the sensor to normal mode.
   *
   * If initialization or configuration fails, the temporary I2C device is
   * removed and internal state is reset.
   *
   * @param bus ESP-IDF I2C master bus handle.
   * @param address 7-bit I2C address to try.
   * @return true if the sensor was successfully initialized at @p address,
   * false otherwise.
   */
  bool init_at_address(i2c_master_bus_handle_t bus, uint8_t address);

  /**
   * @brief FreeRTOS task entry point for periodic sensor updates.
   *
   * @param arg Pointer to the bme280 instance that owns the task.
   */
  static void api_task_entry(void *arg);

  /**
   * @brief Bosch BME280 I2C read callback.
   *
   * Performs an ESP-IDF i2c_master_transmit_receive() transaction with a
   * 100 ms timeout.
   *
   * @param reg_addr Register address to read from.
   * @param reg_data Destination buffer for read data.
   * @param len Number of bytes to read.
   * @param intf_ptr Pointer to an i2c_master_dev_handle_t used by the callback.
   * @return BME280_INTF_RET_SUCCESS on success, BME280_E_NULL_PTR for invalid
   * pointers, or BME280_E_COMM_FAIL on I2C communication failure.
   */
  static BME280_INTF_RET_TYPE i2c_read(uint8_t reg_addr, uint8_t *reg_data,
                                       uint32_t len, void *intf_ptr);

  /**
   * @brief Bosch BME280 I2C write callback.
   *
   * Writes the register address followed by payload data using ESP-IDF
   * i2c_master_transmit() with a 100 ms timeout. Writes larger than the local
   * 32-byte buffer are rejected.
   *
   * @param reg_addr Register address to write to.
   * @param reg_data Source buffer containing data to write.
   * @param len Number of data bytes to write.
   * @param intf_ptr Pointer to an i2c_master_dev_handle_t used by the callback.
   * @return BME280_INTF_RET_SUCCESS on success, BME280_E_NULL_PTR for invalid
   * pointers, or BME280_E_COMM_FAIL on I2C communication failure or oversized
   * write.
   */
  static BME280_INTF_RET_TYPE i2c_write(uint8_t reg_addr,
                                        const uint8_t *reg_data, uint32_t len,
                                        void *intf_ptr);

  /**
   * @brief Bosch BME280 delay callback.
   *
   * Uses esp_rom_delay_us() to block for the requested number of microseconds.
   *
   * @param period Delay duration in microseconds.
   * @param intf_ptr Unused interface context pointer.
   */
  static void delay_us(uint32_t period, void *intf_ptr);
};
