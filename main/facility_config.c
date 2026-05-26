#include "facility_config.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

static Facility_Config s_cfg;
static SemaphoreHandle_t s_mutex;

esp_err_t facility_config_init(void) {
  s_mutex = xSemaphoreCreateMutex();

  if (!s_mutex) {
    return ESP_FAIL;
  }
  memset(&s_cfg, 0, sizeof(s_cfg));

  return facility_config_load(&s_cfg);
}

esp_err_t facility_config_load(Facility_Config *cfg_out) {
  if (!cfg_out)
    return ESP_ERR_INVALID_ARG;

  if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
    nvs_handle_t handle;

    esp_err_t err = nvs_open("facility", NVS_READONLY, &handle);

    if (err != ESP_OK) {
      xSemaphoreGive(s_mutex);
      return err;
    }

    memset(cfg_out, 0, sizeof(Facility_Config));

    size_t len;

    len = sizeof(cfg_out->facility_name);
    nvs_get_str(handle, "facility_name", cfg_out->facility_name, &len);

    len = sizeof(cfg_out->lat);
    nvs_get_str(handle, "lat", cfg_out->lat, &len);

    len = sizeof(cfg_out->lon);
    nvs_get_str(handle, "lon", cfg_out->lon, &len);

    len = sizeof(cfg_out->address);
    nvs_get_str(handle, "address", cfg_out->address, &len);

    len = sizeof(cfg_out->city);
    nvs_get_str(handle, "city", cfg_out->city, &len);

    nvs_get_u8(handle, "zip", &cfg_out->zip);
    nvs_get_u8(handle, "energy_zone", &cfg_out->energy_zone);

    nvs_close(handle);
    xSemaphoreGive(s_mutex);
    return ESP_OK;
  }

  return ESP_ERR_TIMEOUT;
}

esp_err_t facility_config_set_all(const Facility_Config *cfg) {
  if (!cfg)
    return ESP_ERR_INVALID_ARG;

  if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
    nvs_handle_t handle;

    esp_err_t err = nvs_open("facility", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
      xSemaphoreGive(s_mutex);
      return err;
    }

    nvs_set_str(handle, "facility_name", cfg->facility_name);
    nvs_set_str(handle, "lat", cfg->lat);
    nvs_set_str(handle, "lon", cfg->lon);
    nvs_set_str(handle, "address", cfg->address);
    nvs_set_str(handle, "city", cfg->city);
    nvs_set_u8(handle, "zip", cfg->zip);
    nvs_set_u8(handle, "energy_zone", cfg->energy_zone);

    err = nvs_commit(handle);

    nvs_close(handle);
    xSemaphoreGive(s_mutex);
    return err;
  }
  return ESP_ERR_TIMEOUT;
}

esp_err_t facility_config_get_str_field(const char *key, char *out,
                                        size_t len) {
  if (!out || len < 1)
    return ESP_ERR_INVALID_ARG;

  if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("facility", NVS_READONLY, &handle);

    if (err != ESP_OK) {
      xSemaphoreGive(s_mutex);
      return err;
    }

    nvs_get_str(handle, key, out, &len);

    nvs_close(handle);
    xSemaphoreGive(s_mutex);
    return ESP_OK;
  }
  return ESP_ERR_TIMEOUT;
}

esp_err_t facility_config_set_str_field(const char *key, const char *value) {

  if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("facility", NVS_READWRITE, &handle);

    if (err != ESP_OK) {
      xSemaphoreGive(s_mutex);
      return err;
    }
    nvs_set_str(handle, key, value);
    err = nvs_commit(handle);
    nvs_close(handle);
    xSemaphoreGive(s_mutex);
    return err;
  }
  return ESP_ERR_TIMEOUT;
}

esp_err_t facility_config_set_int_field(const char *key, uint8_t value) {

  if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {

    nvs_handle_t handle;
    esp_err_t err = nvs_open("facility", NVS_READWRITE, &handle);

    if (err != ESP_OK) {
      xSemaphoreGive(s_mutex);
      return err;
    }

    nvs_set_u8(handle, key, value);
    err = nvs_commit(handle);
    nvs_close(handle);
    xSemaphoreGive(s_mutex);
    return err;
  }
  return ESP_ERR_TIMEOUT;
}

esp_err_t facility_config_get_int_field(const char *key, uint8_t *out) {

  if (xSemaphoreTake(s_mutex, 0) == pdTRUE) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("facility", NVS_READONLY, &handle);

    if (err != ESP_OK) {
      xSemaphoreGive(s_mutex);
      return err;
    }

    nvs_get_u8(handle, key, out);
    nvs_close(handle);
    xSemaphoreGive(s_mutex);
    return ESP_OK;
  }
  return ESP_ERR_TIMEOUT;
}
