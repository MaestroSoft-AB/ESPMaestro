#pragma once

#include "../components/bme280_driver/include/bme280.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

/**
 * @brief I2C context passed to the Bosch BME280 driver callbacks.
 *
 * Bundles the ESP-IDF I2C device handle and a FreeRTOS mutex so that the
 * static read/write callbacks can share the I2C bus safely across tasks.
 */
struct bme280_i2c_context {
  /** @brief ESP-IDF handle for the registered I2C device. */
  i2c_master_dev_handle_t dev;

  /** @brief Mutex that guards all I2C transactions on the shared bus. */
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
 * The class probes the standard BME280 I2C addresses (0x77 then 0x76),
 * configures the Bosch BME280 driver in normal mode, stores the latest
 * measurement, and can run a FreeRTOS task that periodically updates the
 * cached reading.
 *
 * On start(), any previously persisted reading is restored from NVS and
 * forwarded to the display before the first live sensor read completes.
 *
 * ### Typical usage
 * @code
 * bme280 sensor;
 * sensor.start(bus, i2c_mutex, 2000);   // background task, 2 s interval
 *
 * bme280_reading r;
 * if (sensor.latest(&r)) {
 *     printf("%.1f °C\n", r.temperature_c);
 * }
 * @endcode
 */
class bme280 {
public:
  /**
   * @brief Construct an uninitialized BME280 sensor wrapper.
   *
   * All internal handles are null and @p initialized_ is false. Call init()
   * or start() before reading sensor data.
   */
  bme280() = default;

  /**
   * @brief Initialize the BME280 sensor on an I2C master bus.
   *
   * If already initialized, returns true immediately. Otherwise probes address
   * 0x77 first, then 0x76. On success the sensor is configured with:
   * - Humidity oversampling: 1×
   * - Pressure oversampling: 16×
   * - Temperature oversampling: 2×
   * - IIR filter coefficient: 16
   * - Standby time: 1000 ms
   * - Power mode: normal
   *
   * @param bus ESP-IDF I2C master bus handle to probe.
   * @return true  if the sensor was already initialized or is now initialized.
   * @return false if no BME280 was found or configuration failed.
   */
  bool init(i2c_master_bus_handle_t bus);

  /**
   * @brief Read one measurement from the initialized sensor.
   *
   * Calls the Bosch BME280 driver to retrieve temperature, humidity, and
   * pressure. On success the cached reading is updated, the display handler is
   * notified via display_handler_update_bme280(), and the values are persisted
   * to NVS under the "bme_cache" namespace.
   *
   * @return true  if the measurement was read and cached successfully.
   * @return false if the sensor is not initialized or the Bosch driver call
   *               failed.
   */
  bool read();

  /**
   * @brief Copy the latest valid cached reading.
   *
   * Thread-safe in the sense that it only reads the struct; no mutex is held.
   * The reading is valid only if a successful read() has completed since
   * construction or the last init().
   *
   * @param[out] out Destination that receives a copy of the cached reading.
   * @return true  if @p out is non-null and the cached reading is marked valid.
   * @return false if @p out is null or no valid reading exists yet.
   */
  bool latest(bme280_reading *out) const;

  /**
   * @brief Convert raw Bosch BME280 sensor data to a @ref bme280_reading.
   *
   * Copies temperature and humidity directly from @p data and converts
   * pressure from pascals to hectopascals. Marks the result valid and
   * stores the supplied Unix timestamp.
   *
   * @param data  Raw sensor data returned by bme280_get_sensor_data().
   * @param epoch Unix epoch timestamp (from time(nullptr)) for the reading.
   * @return Populated and valid @ref bme280_reading.
   */
  static bme280_reading make_reading(const struct bme280_data &data,
                                     uint32_t epoch);

  /**
   * @brief Start a background FreeRTOS task for periodic sensor readings.
   *
   * Before creating the task, any reading previously persisted in NVS is
   * loaded into the cache and forwarded to the display handler so that stale
   * data is available immediately on boot.
   *
   * The task:
   * 1. Delays 5 seconds after startup.
   * 2. Calls init() if the sensor is not yet initialized; on failure retries
   *    every @c BME280_RETRY_MS milliseconds.
   * 3. Calls read() and then waits @p interval_ms milliseconds between reads.
   *
   * Calling this function while the task is already running returns true
   * without creating another task.
   *
   * @param bus         ESP-IDF I2C master bus handle used by the background
   *                    task.
   * @param i2c_mutex   Mutex that guards shared I2C bus access from the task.
   * @param interval_ms Delay in milliseconds between successive read() calls
   *                    (default: 2000).
   * @return true  if the task was already running or was created successfully.
   * @return false if xTaskCreate() failed.
   */
  bool start(i2c_master_bus_handle_t bus, SemaphoreHandle_t i2c_mutex,
             uint32_t interval_ms = 2000);

  /**
   * @brief Stop the background FreeRTOS task if it is running.
   *
   * Calls vTaskDelete() on the internal task handle and sets it to null.
   * Sensor initialization state and the cached reading are preserved; a
   * subsequent call to start() will reuse them.
   */
  void stop();

private:
  /** @brief True after successful Bosch driver initialization and
   * configuration. */
  bool initialized_ = false;

  /** @brief Active 7-bit I2C address of the BME280 sensor (0x76 or 0x77). */
  uint8_t address_ = 0;

  /** @brief ESP-IDF I2C master bus handle stored for use by the background
   * task. */
  i2c_master_bus_handle_t bus_ = nullptr;

  /** @brief ESP-IDF I2C device handle registered for the active BME280
   * address. */
  i2c_master_dev_handle_t dev_handle_ = nullptr;

  /** @brief Mutex guarding shared I2C bus access from driver callbacks. */
  SemaphoreHandle_t i2c_mutex_ = nullptr;

  /** @brief I2C context (device handle + mutex) passed to Bosch callbacks. */
  bme280_i2c_context i2c_ctx_ = {};

  /** @brief Bosch BME280 driver context and callback configuration. */
  struct bme280_dev bosch_dev_ = {};

  /** @brief Last successfully cached sensor reading. */
  bme280_reading latest_ = {};

  /** @brief FreeRTOS task handle for periodic BME280 updates, or null if the
   * task is not running. */
  TaskHandle_t api_task_ = nullptr;

  /** @brief Interval in milliseconds between successive read() calls in the
   * background task. */
  uint32_t api_interval_ms_ = 2000;

  /**
   * @brief Try to initialize and configure the sensor at a specific I2C
   * address.
   *
   * Steps performed:
   * 1. Adds an ESP-IDF I2C device to @p bus at @p address.
   * 2. Populates the Bosch @c bme280_dev struct with read/write/delay
   *    callbacks and the @ref bme280_i2c_context.
   * 3. Calls bme280_init() to verify the chip-ID and load calibration data.
   * 4. Applies the standard sensor settings (oversampling, filter, standby).
   * 5. Switches the sensor to normal power mode.
   *
   * On any failure the temporary I2C device is removed from the bus and all
   * related internal state is reset.
   *
   * @param bus     ESP-IDF I2C master bus handle.
   * @param address 7-bit I2C address to try (0x76 or 0x77).
   * @return true  if the sensor was successfully initialized at @p address.
   * @return false if device registration, chip verification, or configuration
   *               failed.
   */
  bool init_at_address(i2c_master_bus_handle_t bus, uint8_t address);

  /**
   * @brief FreeRTOS task entry point for periodic sensor updates.
   *
   * Casts @p arg to a @c bme280* and runs the init-and-read loop described
   * in start(). This function never returns; the task terminates only when
   * stop() calls vTaskDelete().
   *
   * @param arg Pointer to the @c bme280 instance that owns the task.
   */
  static void api_task_entry(void *arg);

  /**
   * @brief Bosch BME280 I2C read callback.
   *
   * Acquires @c bme280_i2c_context::mutex (up to 1000 ms), then performs a
   * combined write-read transaction via i2c_master_transmit_receive() with a
   * 100 ms I2C timeout.
   *
   * @param reg_addr  Register address to read from.
   * @param reg_data  Destination buffer for the read data.
   * @param len       Number of bytes to read.
   * @param intf_ptr  Pointer to a @ref bme280_i2c_context.
   * @return @c BME280_INTF_RET_SUCCESS on success.
   * @return @c BME280_E_NULL_PTR      if @p intf_ptr or @p reg_data is null,
   *                                   or if the context fields are null.
   * @return @c BME280_E_COMM_FAIL     if the mutex could not be acquired or
   *                                   the I2C transaction failed.
   */
  static BME280_INTF_RET_TYPE i2c_read(uint8_t reg_addr, uint8_t *reg_data,
                                       uint32_t len, void *intf_ptr);

  /**
   * @brief Bosch BME280 I2C write callback.
   *
   * Assembles a local 32-byte buffer containing @p reg_addr followed by
   * @p reg_data, acquires @c bme280_i2c_context::mutex (up to 1000 ms), then
   * writes the buffer via i2c_master_transmit() with a 100 ms I2C timeout.
   * Writes that would exceed the 32-byte buffer are rejected immediately.
   *
   * @param reg_addr  Register address to write to.
   * @param reg_data  Source buffer containing data to write.
   * @param len       Number of data bytes to write (must satisfy len + 1 ≤ 32).
   * @param intf_ptr  Pointer to a @ref bme280_i2c_context.
   * @return @c BME280_INTF_RET_SUCCESS on success.
   * @return @c BME280_E_NULL_PTR      if @p intf_ptr or @p reg_data is null,
   *                                   or if the context fields are null.
   * @return @c BME280_E_COMM_FAIL     if @p len + 1 > 32, the mutex could not
   *                                   be acquired, or the I2C transaction
   *                                   failed.
   */
  static BME280_INTF_RET_TYPE i2c_write(uint8_t reg_addr,
                                        const uint8_t *reg_data, uint32_t len,
                                        void *intf_ptr);

  /**
   * @brief Bosch BME280 microsecond delay callback.
   *
   * Blocks the calling task for @p period microseconds using
   * esp_rom_delay_us(). This is a busy-wait and should only be called during
   * initialization or measurement startup, not from a high-priority task.
   *
   * @param period    Delay duration in microseconds.
   * @param intf_ptr  Unused; present only to satisfy the Bosch driver
   *                  callback signature.
   */
  static void delay_us(uint32_t period, void *intf_ptr);
};
