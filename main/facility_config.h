#ifndef __FACILITY_CONFIG_H__
#define __FACILITY_CONFIG_H__
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>

/**
 * @brief Persistent facility configuration.
 *
 * Stores the local facility identity and location data used by the system.
 * The configuration is persisted in ESP-IDF NVS under the "facility"
 * namespace using the keys "facility_name", "lat", "lon", and "energy_zone".
 *
 * A configuration is considered complete when @p facility_name, @p lat, and
 * @p lon are all non-empty and @p energy_zone is in the range 1–4. Use
 * facility_config_is_configured() to check completeness.
 *
 * @see facility_config_set_all()
 * @see facility_config_is_configured()
 */
typedef struct {
  /** @brief Human-readable facility name (null-terminated, max 31 chars). */
  char facility_name[32];

  /** @brief Geographic latitude as a null-terminated decimal string
   *         (e.g. "59.3293"). Max 15 chars. */
  char lat[16];

  /** @brief Geographic longitude as a null-terminated decimal string
   *         (e.g. "18.0686"). Max 15 chars. */
  char lon[16];

  /** @brief Swedish electricity price zone in the range 1–4 (SE1–SE4). */
  uint8_t energy_zone;
} Facility_Config;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the facility configuration module.
 *
 * Must be called once before any other facility_config_* function. Performs
 * the following in order:
 * 1. Creates the internal mutex used to serialize NVS access.
 * 2. Creates a depth-1 queue and a background task for asynchronous
 *    synchronization of configuration changes to the optimizer service.
 *    If either fails, a warning is logged and sync is disabled; the rest of
 *    the module continues to function.
 * 3. Clears the in-memory configuration cache.
 * 4. Loads the current configuration from NVS into the cache via
 *    facility_config_load().
 *
 * @return ESP_OK     on success.
 * @return ESP_FAIL   if the internal mutex could not be created.
 * @return other      ESP-IDF error code from facility_config_load() if the
 *                    initial NVS load fails (e.g. ESP_ERR_NVS_NOT_FOUND on
 *                    first boot).
 */
esp_err_t facility_config_init(void);

/**
 * @brief Load the facility configuration from NVS into @p cfg_out.
 *
 * Opens the "facility" NVS namespace in read-only mode and reads all four
 * fields (facility_name, lat, lon, energy_zone) into @p cfg_out. Fields
 * that are not yet stored in NVS are left as zero/empty without causing an
 * error return. The destination structure is zeroed before reading.
 *
 * This function is safe to call at any time after facility_config_init().
 *
 * @param[out] cfg_out Destination that receives the loaded configuration.
 * @return ESP_OK              on success.
 * @return ESP_ERR_INVALID_ARG if @p cfg_out is null.
 * @return ESP_ERR_TIMEOUT     if the internal mutex could not be acquired.
 * @return other               ESP-IDF error code if the NVS namespace could
 *                             not be opened.
 */
esp_err_t facility_config_load(Facility_Config *cfg_out);

/**
 * @brief Check whether a complete facility configuration is stored in NVS.
 *
 * Calls facility_config_load() and verifies that facility_name, lat, and lon
 * are all non-empty and that energy_zone is in the range 1–4.
 *
 * @return true  if a complete configuration is present.
 * @return false if loading fails or any required field is missing or invalid.
 */
bool facility_config_is_configured(void);

/**
 * @brief Save a facility configuration to NVS and trigger background sync.
 *
 * Opens the "facility" NVS namespace in read-write mode, writes all four
 * fields, and commits the changes. On successful commit:
 * - The in-memory configuration cache is updated.
 * - The configuration is queued for asynchronous HTTP POST to the optimizer
 *   service (see OPTIMAESTRO_FACILITY_CONFIG_URL). If Wi-Fi is not connected
 *   at that point the sync is silently skipped.
 * - A dashboard data refresh is requested with zero delay via
 *   dashboard_data_request_refresh().
 *
 * @param cfg Configuration to persist. All fields are written as-is; no
 *            validation is performed before writing.
 * @return ESP_OK              on success.
 * @return ESP_ERR_INVALID_ARG if @p cfg is null.
 * @return ESP_ERR_TIMEOUT     if the internal mutex could not be acquired.
 * @return other               ESP-IDF error code from NVS commit.
 */
esp_err_t facility_config_set_all(const Facility_Config *cfg);

/**
 * @brief Read a single string field from the "facility" NVS namespace.
 *
 * A thin wrapper around nvs_get_str() with mutex protection. Intended for
 * reading individual fields without loading the full configuration. Known
 * keys are "facility_name", "lat", and "lon".
 *
 * @param key       NVS key to read (null-terminated).
 * @param[out] out  Destination buffer for the string value.
 * @param len       Size of @p out in bytes; the result is truncated to fit.
 * @return ESP_OK              on success.
 * @return ESP_ERR_INVALID_ARG if @p out is null or @p len is zero.
 * @return ESP_ERR_TIMEOUT     if the internal mutex could not be acquired.
 * @return other               ESP-IDF error code from NVS (e.g.
 *                             ESP_ERR_NVS_NOT_FOUND if the key does not
 *                             exist).
 */
esp_err_t facility_config_get_str_field(const char *key, char *out, size_t len);

/**
 * @brief Write a single string field to the "facility" NVS namespace.
 *
 * A thin wrapper around nvs_set_str() + nvs_commit() with mutex protection.
 * Intended for updating individual fields without rewriting the full
 * configuration. Known keys are "facility_name", "lat", and "lon".
 *
 * Note: unlike facility_config_set_all(), this function does not update the
 * in-memory cache or trigger optimizer sync.
 *
 * @param key   NVS key to write (null-terminated).
 * @param value Null-terminated string value to store.
 * @return ESP_OK          on success.
 * @return ESP_ERR_TIMEOUT if the internal mutex could not be acquired.
 * @return other           ESP-IDF error code from NVS.
 */
esp_err_t facility_config_set_str_field(const char *key, const char *value);

/**
 * @brief Write a single uint8_t field to the "facility" NVS namespace.
 *
 * A thin wrapper around nvs_set_u8() + nvs_commit() with mutex protection.
 * Primarily used for the "energy_zone" key.
 *
 * Note: unlike facility_config_set_all(), this function does not update the
 * in-memory cache or trigger optimizer sync.
 *
 * @param key   NVS key to write (null-terminated).
 * @param value 8-bit unsigned value to store.
 * @return ESP_OK          on success.
 * @return ESP_ERR_TIMEOUT if the internal mutex could not be acquired.
 * @return other           ESP-IDF error code from NVS.
 */
esp_err_t facility_config_set_int_field(const char *key, uint8_t value);

/**
 * @brief Read a single uint8_t field from the "facility" NVS namespace.
 *
 * A thin wrapper around nvs_get_u8() with mutex protection. Primarily used
 * for the "energy_zone" key.
 *
 * @param key       NVS key to read (null-terminated).
 * @param[out] out  Destination pointer for the loaded value.
 * @return ESP_OK              on success.
 * @return ESP_ERR_TIMEOUT     if the internal mutex could not be acquired.
 * @return other               ESP-IDF error code from NVS (e.g.
 *                             ESP_ERR_NVS_NOT_FOUND if the key does not
 *                             exist).
 */
esp_err_t facility_config_get_int_field(const char *key, uint8_t *out);

#ifdef __cplusplus
}
#endif
#endif
