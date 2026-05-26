#ifndef __FACILITY_CONFIG_H__
#define __FACILITY_CONFIG_H__
#include "freertos/FreeRTOS.h"

typedef struct {
  char facility_name[32];
  char lat[16];
  char lon[16];
  char address[32];
  char city[32];
  uint8_t zip;
  uint8_t energy_zone;
} Facility_Config;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t facility_config_init(void);
esp_err_t facility_config_load(Facility_Config *cfg);
esp_err_t facility_config_set_all(const Facility_Config *cfg);
esp_err_t facility_config_get_str_field(const char *key, char *out, size_t len);
esp_err_t facility_config_set_str_field(const char *key, const char *value);
esp_err_t facility_config_set_int_field(const char *key, uint8_t value);
esp_err_t facility_config_get_int_field(const char *key, uint8_t *out);
#ifdef __cplusplus
}
#endif
#endif
