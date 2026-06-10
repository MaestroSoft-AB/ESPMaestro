#ifndef __FACILITY_CONFIG_H__
#define __FACILITY_CONFIG_H__
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>
/**
 * @brief Persistent facility configuration.
 *
 * Stores the local facility identity and location data used by the system.
 *
 * The configuration is saved in ESP-IDF NVS under the "facility" namespace.
 * A configuration is considered complete when facility_name, lat, and lon are
 * non-empty, and energy_zone is in the range 1-4.
 */
typedef struct {
  /** @brief Facility name. */
  char facility_name[32];

  /** @brief Latitude as a null-terminated string. */
  char lat[16];

  /** @brief Longitude as a null-terminated string. */
  char lon[16];

  /** @brief Energy price zone, expected to be in the range 1-4. */
  uint8_t energy_zone;
} Facility_Config;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the facility configuration module.
 *
 * Creates the internal mutex, creates the optional optimizer sync queue and
 * background sync task, clears the cached configuration, and loads the saved
 * configuration from NVS.
 *
 * @return ESP_OK if initialization and loading succeeded, otherwise an ESP-IDF
 * error code.
 */
esp_err_t facility_config_init(void);

/**
 * @brief Load the facility configuration from NVS.
 *
 * Reads facility_name, lat, lon, and energy_zone from the "facility" NVS
 * namespace.
 *
 * @param cfg Destination structure that receives the loaded configuration.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if @p cfg is null,
 * ESP_ERR_TIMEOUT if the internal mutex could not be taken, or another ESP-IDF
 * error code from NVS.
 */
esp_err_t facility_config_load(Facility_Config *cfg);

/**
 * @brief Check whether a complete facility configuration is stored.
 *
 * A configuration is complete when facility_name, lat, and lon are non-empty,
 * and energy_zone is in the range 1-4.
 *
 * @return true if a complete configuration could be loaded, false otherwise.
 */
bool facility_config_is_configured(void);

/**
 * @brief Save a complete facility configuration to NVS.
 *
 * Writes facility_name, lat, lon, and energy_zone to the "facility" NVS
 * namespace. On successful commit, the configuration is cached internally and
 * queued for background synchronization with the optimizer service.
 *
 * @param cfg Configuration to save.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if @p cfg is null,
 * ESP_ERR_TIMEOUT if the internal mutex could not be taken, or another ESP-IDF
 * error code from NVS.
 */
esp_err_t facility_config_set_all(const Facility_Config *cfg);

/**
 * @brief Read a string field from the facility NVS namespace.
 *
 * Typical string keys are "facility_name", "lat", and "lon".
 *
 * @param key NVS key to read.
 * @param out Destination buffer.
 * @param len Size of @p out in bytes.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if @p out is null or
 * @p len is zero, ESP_ERR_TIMEOUT if the internal mutex could not be taken,
 * or another ESP-IDF error code from NVS.
 */
esp_err_t facility_config_get_str_field(const char *key, char *out, size_t len);

/**
 * @brief Write a string field to the facility NVS namespace.
 *
 * Typical string keys are "facility_name", "lat", and "lon".
 *
 * @param key NVS key to write.
 * @param value Null-terminated string value to store.
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if the internal mutex could not be
 * taken, or another ESP-IDF error code from NVS.
 */
esp_err_t facility_config_set_str_field(const char *key, const char *value);

/**
 * @brief Write an integer field to the facility NVS namespace.
 *
 * This is primarily used for the "energy_zone" key.
 *
 * @param key NVS key to write.
 * @param value 8-bit value to store.
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if the internal mutex could not be
 * taken, or another ESP-IDF error code from NVS.
 */
esp_err_t facility_config_set_int_field(const char *key, uint8_t value);

/**
 * @brief Read an integer field from the facility NVS namespace.
 *
 * This is primarily used for the "energy_zone" key.
 *
 * @param key NVS key to read.
 * @param out Destination pointer for the loaded value.
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if the internal mutex could not be
 * taken, or another ESP-IDF error code from NVS.
 */
esp_err_t facility_config_get_int_field(const char *key, uint8_t *out);
#ifdef __cplusplus
}
#endif
#endif
