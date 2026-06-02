#include "facility_config.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "wifi_handler.h"
#include <string.h>

static const char *TAG = "facility_config";

#ifndef OPTIMAESTRO_FACILITY_CONFIG_URL
#define OPTIMAESTRO_FACILITY_CONFIG_URL                                          \
  "http://135.225.131.246:10580/api/v1/config"
#endif

#define FACILITY_CONFIG_HTTP_TIMEOUT_MS 5000
#define FACILITY_CONFIG_SYNC_TASK_STACK 6144
#define FACILITY_CONFIG_SYNC_TASK_PRIORITY 2
#define FACILITY_CONFIG_BODY_MAX 384
#define FACILITY_CONFIG_URL_MAX 256
#define FACILITY_CONFIG_NAME_ENCODED_MAX 96

static Facility_Config s_cfg;
static SemaphoreHandle_t s_mutex;
static QueueHandle_t s_sync_queue;
static TaskHandle_t s_sync_task;

static void facility_config_sync_task(void *arg);
static esp_err_t facility_config_post_to_optimizer(const Facility_Config *cfg);
static void facility_config_queue_sync(const Facility_Config *cfg);

static bool facility_config_complete(const Facility_Config *cfg) {
  if (!cfg)
    return false;

  return cfg->facility_name[0] != '\0' && cfg->lat[0] != '\0' &&
         cfg->lon[0] != '\0' && cfg->energy_zone >= 1 &&
         cfg->energy_zone <= 4;
}

esp_err_t facility_config_init(void) {
  s_mutex = xSemaphoreCreateMutex();

  if (!s_mutex) {
    return ESP_FAIL;
  }

  s_sync_queue = xQueueCreate(1, sizeof(Facility_Config));
  if (!s_sync_queue) {
    ESP_LOGW(TAG, "Failed to create optimizer sync queue");
  } else if (xTaskCreate(facility_config_sync_task, "facility_cfg_sync",
                         FACILITY_CONFIG_SYNC_TASK_STACK, NULL,
                         FACILITY_CONFIG_SYNC_TASK_PRIORITY,
                         &s_sync_task) != pdPASS) {
    ESP_LOGW(TAG, "Failed to create optimizer sync task");
    vQueueDelete(s_sync_queue);
    s_sync_queue = NULL;
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

    nvs_get_u8(handle, "energy_zone", &cfg_out->energy_zone);

    nvs_close(handle);
    xSemaphoreGive(s_mutex);
    return ESP_OK;
  }

  return ESP_ERR_TIMEOUT;
}

bool facility_config_is_configured(void) {
  Facility_Config cfg;

  if (facility_config_load(&cfg) != ESP_OK)
    return false;

  return facility_config_complete(&cfg);
}

esp_err_t facility_config_set_all(const Facility_Config *cfg) {
  if (!cfg)
    return ESP_ERR_INVALID_ARG;

  Facility_Config saved_cfg = *cfg;

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
    nvs_set_u8(handle, "energy_zone", cfg->energy_zone);

    err = nvs_commit(handle);

    nvs_close(handle);
    xSemaphoreGive(s_mutex);
    if (err == ESP_OK) {
      s_cfg = saved_cfg;
      facility_config_queue_sync(&saved_cfg);
    }
    return err;
  }
  return ESP_ERR_TIMEOUT;
}

static void facility_config_queue_sync(const Facility_Config *cfg) {
  if (!cfg || !s_sync_queue)
    return;

  xQueueOverwrite(s_sync_queue, cfg);
}

static void facility_config_url_encode(const char *in, char *out,
                                       size_t out_size) {
  static const char hex[] = "0123456789ABCDEF";
  size_t used = 0;

  if (!out || out_size == 0)
    return;

  if (!in) {
    out[0] = '\0';
    return;
  }

  while (*in && used + 1 < out_size) {
    unsigned char ch = (unsigned char)*in++;
    bool plain = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                 (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
                 ch == '.' || ch == '~';

    if (plain) {
      out[used++] = (char)ch;
    } else if (used + 3 < out_size) {
      out[used++] = '%';
      out[used++] = hex[ch >> 4];
      out[used++] = hex[ch & 0x0f];
    } else {
      break;
    }
  }

  out[used] = '\0';
}

static esp_err_t facility_config_post_to_optimizer(const Facility_Config *cfg) {
  if (!facility_config_complete(cfg))
    return ESP_ERR_INVALID_ARG;

  char encoded_name[FACILITY_CONFIG_NAME_ENCODED_MAX] = {0};
  char url[FACILITY_CONFIG_URL_MAX] = {0};
  char body[FACILITY_CONFIG_BODY_MAX] = {0};

  facility_config_url_encode(cfg->facility_name, encoded_name,
                             sizeof(encoded_name));

  snprintf(url, sizeof(url), "%s?name=%s", OPTIMAESTRO_FACILITY_CONFIG_URL,
           encoded_name);

  snprintf(body, sizeof(body),
           "name=%s\n"
           "currency=SEK\n"
           "energy_zone=%u\n"
           "latitude=%s\n"
           "longitude=%s\n",
           cfg->facility_name, cfg->energy_zone, cfg->lat, cfg->lon);

  esp_http_client_config_t config = {};
  config.url = url;
  config.timeout_ms = FACILITY_CONFIG_HTTP_TIMEOUT_MS;
  config.method = HTTP_METHOD_POST;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) {
    return ESP_FAIL;
  }

  esp_http_client_set_header(client, "Content-Type",
                             "text/plain; charset=utf-8");
  esp_http_client_set_post_field(client, body, strlen(body));

  esp_err_t err = esp_http_client_perform(client);
  int status = esp_http_client_get_status_code(client);

  if (err == ESP_OK && status >= 200 && status < 300) {
    ESP_LOGI(TAG, "Facility config posted to optimizer: status=%d", status);
  } else {
    ESP_LOGW(TAG, "Facility config post failed: url=%s err=%s status=%d", url,
             esp_err_to_name(err), status);
    if (err == ESP_OK)
      err = ESP_FAIL;
  }

  esp_http_client_cleanup(client);
  return err;
}

static void facility_config_sync_task(void *arg) {
  (void)arg;
  Facility_Config cfg;

  while (1) {
    if (xQueueReceive(s_sync_queue, &cfg, portMAX_DELAY) != pdTRUE)
      continue;

    if (!wifi_handler_is_connected()) {
      ESP_LOGW(TAG, "Skipping optimizer config sync: WiFi not connected");
      continue;
    }

    facility_config_post_to_optimizer(&cfg);
  }
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
