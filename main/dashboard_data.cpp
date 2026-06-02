#include "dashboard_data.hpp"
#include "display_handler.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "wifi_handler.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "DashboardData";

#ifndef OPTIMAESTRO_DISPLAY_GRAPH_URL
#define OPTIMAESTRO_DISPLAY_GRAPH_URL                                             \
  "http://135.225.131.246:10580/api/v1/display/graph/hour"
#endif

#define OPTIMAESTRO_PROFILE_BUCKETS 96
#define OPTIMAESTRO_BUCKETS_PER_HOUR 4
#define OPTIMAESTRO_FETCH_INTERVAL_MS 90000
#define OPTIMAESTRO_FETCH_RETRY_MS 15000
#define OPTIMAESTRO_HTTP_TIMEOUT_MS 5000
#define OPTIMAESTRO_HTTP_MAX_BODY 8192
#define OPTIMAESTRO_HTTP_LOG_BODY_MAX 240
#define OPTIMAESTRO_FETCH_TASK_STACK 8192
#define OPTIMAESTRO_FETCH_TASK_PRIORITY 2

typedef struct {
  char *data;
  int len;
  int capacity;
} HttpBuffer;

typedef struct {
  bool success;
  float current_sek_kwh;
  float sek_24h[24];
  uint32_t power_w;
  uint32_t max_power_w_24h;
  float current_kwh;
  float current_sek_h;
  uint32_t power_24h[24];
  float kwh_24h[24];
  float cost_24h[24];
} DbdFetchResult;

/*------------------------------------*/
static void dbd_taskwork(void *_context, uint64_t _now);
static void dbd_fetch_worker_task(void *_context);
/*------------------------------------*/

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
  if (evt->event_id != HTTP_EVENT_ON_DATA || evt->data == NULL ||
      evt->data_len <= 0) {
    return ESP_OK;
  }

  HttpBuffer *buffer = (HttpBuffer *)evt->user_data;
  if (buffer == NULL || buffer->data == NULL)
    return ESP_FAIL;

  if (buffer->len + evt->data_len >= buffer->capacity) {
    ESP_LOGW(TAG, "OptiMaestro response too large");
    return ESP_FAIL;
  }

  memcpy(buffer->data + buffer->len, evt->data, evt->data_len);
  buffer->len += evt->data_len;
  buffer->data[buffer->len] = '\0';
  return ESP_OK;
}

static bool read_number_array_item(cJSON *array, int index, double *out) {
  if (array == NULL || out == NULL)
    return false;

  cJSON *item = cJSON_GetArrayItem(array, index);
  if (!cJSON_IsNumber(item))
    return false;

  *out = item->valuedouble;
  return true;
}

static bool dbd_parse_optimaestro_display_json(const char *json,
                                               DbdFetchResult *result) {
  if (json == NULL || result == NULL)
    return false;

  cJSON *root = cJSON_Parse(json);
  if (root == NULL)
    return false;

  cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "v");
  cJSON *series = cJSON_GetObjectItemCaseSensitive(root, "s");
  cJSON *summary = cJSON_GetObjectItemCaseSensitive(root, "sum");
  cJSON *consumption = cJSON_GetObjectItemCaseSensitive(series, "c");
  cJSON *price = cJSON_GetObjectItemCaseSensitive(series, "p");

  if (!cJSON_IsString(version) || strcmp(version->valuestring, "od1") != 0 ||
      !cJSON_IsArray(consumption) || !cJSON_IsArray(price) ||
      cJSON_GetArraySize(consumption) < OPTIMAESTRO_PROFILE_BUCKETS ||
      cJSON_GetArraySize(price) < OPTIMAESTRO_PROFILE_BUCKETS) {
    cJSON_Delete(root);
    return false;
  }

  float sek_24h[24] = {0};
  uint32_t power_24h[24] = {0};
  float kwh_24h[24] = {0};
  float cost_24h[24] = {0};

  float total_kwh = 0.0f;
  float total_cost = 0.0f;
  uint32_t max_power = 0;

  for (int hour = 0; hour < 24; hour++) {
    double hour_mwh = 0.0;
    double hour_cost_sek = 0.0;
    double hour_price_msek = 0.0;
    int valid_buckets = 0;

    for (int bucket = 0; bucket < OPTIMAESTRO_BUCKETS_PER_HOUR; bucket++) {
      int index = (hour * OPTIMAESTRO_BUCKETS_PER_HOUR) + bucket;
      double bucket_mwh = 0.0;
      double bucket_msek = 0.0;

      if (!read_number_array_item(consumption, index, &bucket_mwh) ||
          !read_number_array_item(price, index, &bucket_msek)) {
        cJSON_Delete(root);
        return false;
      }

      hour_mwh += bucket_mwh;
      hour_price_msek += bucket_msek;
      hour_cost_sek += (bucket_mwh / 1000.0) * (bucket_msek / 1000.0);
      valid_buckets++;
    }

    float hour_kwh = (float)(hour_mwh / 1000.0);
    float hour_cost = (float)hour_cost_sek;
    uint32_t hour_power = (uint32_t)(hour_kwh * 1000.0f + 0.5f);

    kwh_24h[hour] = hour_kwh;
    cost_24h[hour] = hour_cost;
    sek_24h[hour] =
        valid_buckets > 0 ? (float)((hour_price_msek / valid_buckets) / 1000.0)
                          : 0.0f;
    power_24h[hour] = hour_power;

    total_kwh += hour_kwh;
    total_cost += hour_cost;
    if (hour_power > max_power)
      max_power = hour_power;
  }

  if (cJSON_IsObject(summary)) {
    cJSON *ct = cJSON_GetObjectItemCaseSensitive(summary, "ct");
    cJSON *cc = cJSON_GetObjectItemCaseSensitive(summary, "cc");
    if (cJSON_IsNumber(ct))
      total_kwh = (float)(ct->valuedouble / 1000.0);
    if (cJSON_IsNumber(cc))
      total_cost = (float)(cc->valuedouble / 100.0);
  }

  result->success = true;
  result->current_sek_kwh = sek_24h[23];
  memcpy(result->sek_24h, sek_24h, sizeof(result->sek_24h));
  result->power_w = power_24h[23];
  result->max_power_w_24h = max_power;
  result->current_kwh = total_kwh;
  result->current_sek_h = total_cost;
  memcpy(result->power_24h, power_24h, sizeof(result->power_24h));
  memcpy(result->kwh_24h, kwh_24h, sizeof(result->kwh_24h));
  memcpy(result->cost_24h, cost_24h, sizeof(result->cost_24h));

  cJSON_Delete(root);
  return true;
}

static bool dbd_fetch_optimaestro_display_json(DbdFetchResult *result) {
  if (result == NULL)
    return false;

  memset(result, 0, sizeof(*result));

  char *response = (char *)calloc(1, OPTIMAESTRO_HTTP_MAX_BODY);
  if (response == NULL)
    return false;

  HttpBuffer buffer = {
      .data = response,
      .len = 0,
      .capacity = OPTIMAESTRO_HTTP_MAX_BODY,
  };

  esp_http_client_config_t config = {};
  config.url = OPTIMAESTRO_DISPLAY_GRAPH_URL;
  config.timeout_ms = OPTIMAESTRO_HTTP_TIMEOUT_MS;
  config.event_handler = http_event_handler;
  config.user_data = &buffer;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == NULL) {
    free(response);
    return false;
  }

  esp_err_t err = esp_http_client_perform(client);
  int status = esp_http_client_get_status_code(client);

  if (err != ESP_OK || status != 200 || buffer.len == 0) {
    ESP_LOGW(TAG, "OptiMaestro fetch failed: url=%s err=%s status=%d len=%d",
             OPTIMAESTRO_DISPLAY_GRAPH_URL, esp_err_to_name(err), status,
             buffer.len);
    if (buffer.len > 0) {
      int log_len = buffer.len < OPTIMAESTRO_HTTP_LOG_BODY_MAX
                        ? buffer.len
                        : OPTIMAESTRO_HTTP_LOG_BODY_MAX;
      ESP_LOGW(TAG, "OptiMaestro response body: %.*s", log_len, response);
    }
    esp_http_client_cleanup(client);
    free(response);
    return false;
  }

  esp_http_client_cleanup(client);

  bool parsed = dbd_parse_optimaestro_display_json(response, result);
  if (!parsed)
    ESP_LOGW(TAG, "Failed to parse OptiMaestro display data");

  free(response);
  return parsed;
}

/*-----------------------------------*/

DashboardData::DashboardData()
    : initialized_(false), state(DBD_STATE_IDLE), need_weaterdata(true),
      need_electricitydata(true), need_realtimedata(true), task(nullptr),
      fetch_task(NULL), fetch_result_queue(NULL), fetch_in_progress(false),
      base_epoch(0), base_ms(0), next_fetch_ms(0) {
  fetch_result_queue = xQueueCreate(1, sizeof(DbdFetchResult));

  if (fetch_result_queue == NULL) {
    initialized_ = false;
    return;
  }

  BaseType_t task_created =
      xTaskCreate(dbd_fetch_worker_task, "dbd_http_fetch",
                  OPTIMAESTRO_FETCH_TASK_STACK, this,
                  OPTIMAESTRO_FETCH_TASK_PRIORITY, &fetch_task);
  if (task_created != pdPASS) {
    vQueueDelete(fetch_result_queue);
    fetch_result_queue = NULL;
    initialized_ = false;
    return;
  }

  task = scheduler_create_task(this, dbd_taskwork);

  if (task == nullptr) {
    vTaskDelete(fetch_task);
    vQueueDelete(fetch_result_queue);
    fetch_task = NULL;
    fetch_result_queue = NULL;
    initialized_ = false;
    return;
  }

  initialized_ = true;
}

void DashboardData::update_weather(float outdoor_c, float indoor_c,
                                   const char *summary) {

  weatherdata_.valid = true;
  weatherdata_.outdoor_c = outdoor_c;
  weatherdata_.indoor_c = indoor_c;

  snprintf(weatherdata_.summary, sizeof(weatherdata_.summary), "%s",
           summary ? summary : "");

  weatherdata_.updated_epoch = (uint32_t)time(NULL);
}

void DashboardData::update_electricity(float current_sek_kwh,
                                       const float sek_24h[24]) {
  electricitydata_.valid = true;
  electricitydata_.current_sek_kwh = current_sek_kwh;

  memcpy(electricitydata_.sek_24h, sek_24h, sizeof(electricitydata_.sek_24h));

  electricitydata_.updated_epoch = (uint32_t)time(NULL);
}

void DashboardData::update_realtime(uint32_t power_w, uint32_t max_power_w_24h,
                                    float current_kwh, float current_sek_h,
                                    const uint32_t power_24h[24],
                                    const float kwh_24h[24],
                                    const float cost_24h[24]) {
  realtimedata_.valid = true;
  realtimedata_.power_w = power_w;
  realtimedata_.max_power_w_24h = max_power_w_24h;
  realtimedata_.current_kwh = current_kwh;
  realtimedata_.current_sek_h = current_sek_h;

  memcpy(realtimedata_.power_24h, power_24h, sizeof(realtimedata_.power_24h));

  memcpy(realtimedata_.kwh_24h, kwh_24h, sizeof(realtimedata_.kwh_24h));

  memcpy(realtimedata_.cost_24h, cost_24h, sizeof(realtimedata_.cost_24h));

  realtimedata_.updated_epoch = (uint32_t)time(NULL);
}

void DashboardData::send_to_display_handler() {
  display_handler_update_dashboard(&weatherdata_, &electricitydata_,
                                   &realtimedata_);
}

DashboardData::~DashboardData() {
  if (initialized_) {
    scheduler_destroy_task(task);
  }
  if (fetch_task != NULL) {
    vTaskDelete(fetch_task);
    fetch_task = NULL;
  }
  if (fetch_result_queue != NULL) {
    vQueueDelete(fetch_result_queue);
    fetch_result_queue = NULL;
  }
}

/*-------------Taskwork*-------------*/
static void dbd_fetch_worker_task(void *_context) {
  DashboardData *self = static_cast<DashboardData *>(_context);
  DbdFetchResult result = {};

  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    memset(&result, 0, sizeof(result));
    result.success = dbd_fetch_optimaestro_display_json(&result);

    if (self != NULL && self->fetch_result_queue != NULL) {
      xQueueOverwrite(self->fetch_result_queue, &result);
    }
  }
}

static DashboardDataStatus dbd_start_fetch(DashboardData *self,
                                           uint64_t now_ms) {
  if (!wifi_handler_is_connected()) {
    self->next_fetch_ms = now_ms + OPTIMAESTRO_FETCH_RETRY_MS;
    return DBD_STATE_IDLE;
  }

  if (self->need_weaterdata) {
    // TODO: replace with OptiMaestro weather/current endpoint when available.
  }

  if (self->fetch_in_progress) {
    return DBD_STATE_WAIT_RESPONSE;
  }

  DbdFetchResult stale_result = {};
  while (self->fetch_result_queue != NULL &&
         xQueueReceive(self->fetch_result_queue, &stale_result, 0) == pdTRUE) {
  }

  self->fetch_in_progress = true;
  xTaskNotifyGive(self->fetch_task);
  return DBD_STATE_WAIT_RESPONSE;
}

static DashboardDataStatus dbd_wait_response(DashboardData *self,
                                             uint64_t now_ms) {
  DbdFetchResult result = {};

  if (self->fetch_result_queue == NULL ||
      xQueueReceive(self->fetch_result_queue, &result, 0) != pdTRUE) {
    return DBD_STATE_WAIT_RESPONSE;
  }

  self->fetch_in_progress = false;

  if (!result.success) {
    self->next_fetch_ms = now_ms + OPTIMAESTRO_FETCH_RETRY_MS;
    return DBD_STATE_IDLE;
  }

  self->update_electricity(result.current_sek_kwh, result.sek_24h);
  self->update_realtime(result.power_w, result.max_power_w_24h,
                        result.current_kwh, result.current_sek_h,
                        result.power_24h, result.kwh_24h, result.cost_24h);

  self->next_fetch_ms = now_ms + OPTIMAESTRO_FETCH_INTERVAL_MS;
  return DBD_STATE_UPDATE_GRAPHS;
}

static DashboardDataStatus dbd_update_graphs(DashboardData *self,
                                             uint64_t now_ms) {
  // Send data to display handler to update ui
  (void)now_ms;

  self->send_to_display_handler();

  return DBD_STATE_IDLE;
}

static void dbd_taskwork(void *_context, uint64_t _now_ms) {
  DashboardData *self = static_cast<DashboardData *>(_context);
  if (self == nullptr)
    return;

  switch (self->state) {
  case DBD_STATE_INIT:
    ESP_LOGI(TAG, "DBD_STATE_INIT");
    self->next_fetch_ms = _now_ms;
    self->state = DBD_STATE_IDLE;
    break;

  case DBD_STATE_IDLE:
    if (_now_ms >= self->next_fetch_ms) {
      self->state = DBD_STATE_REQUEST_DATA;
    }
    break;

  case DBD_STATE_REQUEST_DATA:
    self->state = dbd_start_fetch(self, _now_ms);
    break;

  case DBD_STATE_WAIT_RESPONSE:
    self->state = dbd_wait_response(self, _now_ms);
    break;

  case DBD_STATE_UPDATE_GRAPHS:
    self->state = dbd_update_graphs(self, _now_ms);
    break;

  default:
    break;
  }
}

/*-----------------------------------*/
