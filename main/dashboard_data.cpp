#include "dashboard_data.hpp"
#include "display_handler.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "wifi_handler.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "DashboardData";

#ifndef OPTIMAESTRO_DISPLAY_GRAPH_URL
#define OPTIMAESTRO_DISPLAY_GRAPH_URL                                             \
  "http://100.68.34.53:10580/api/v1/display/graph/hour"
#endif

#define OPTIMAESTRO_PROFILE_BUCKETS 96
#define OPTIMAESTRO_BUCKETS_PER_HOUR 4
#define OPTIMAESTRO_FETCH_INTERVAL_MS 90000
#define OPTIMAESTRO_FETCH_RETRY_MS 15000
#define OPTIMAESTRO_HTTP_TIMEOUT_MS 5000
#define OPTIMAESTRO_HTTP_MAX_BODY 8192

typedef struct {
  char *data;
  int len;
  int capacity;
} HttpBuffer;

/*------------------------------------*/
static void dbd_taskwork(void *_context, uint64_t _now);
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

static bool dbd_parse_optimaestro_display_json(DashboardData *self,
                                               const char *json) {
  if (self == NULL || json == NULL)
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

  self->update_electricity(sek_24h[23], sek_24h);
  self->update_realtime(power_24h[23], max_power, total_kwh, total_cost,
                        power_24h, kwh_24h, cost_24h);

  cJSON_Delete(root);
  return true;
}

static bool dbd_fetch_optimaestro_display_json(DashboardData *self) {
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
  esp_http_client_cleanup(client);

  if (err != ESP_OK || status != 200 || buffer.len == 0) {
    ESP_LOGW(TAG, "OptiMaestro fetch failed: err=%s status=%d",
             esp_err_to_name(err), status);
    free(response);
    return false;
  }

  bool parsed = dbd_parse_optimaestro_display_json(self, response);
  if (!parsed)
    ESP_LOGW(TAG, "Failed to parse OptiMaestro display data");

  free(response);
  return parsed;
}

/*-----------------------------------*/

DashboardData::DashboardData()
    : initialized_(false), state(DBD_STATE_IDLE), need_weaterdata(true),
      need_electricitydata(true), need_realtimedata(true), base_epoch(0),
      base_ms(0), next_fetch_ms(0) {
  task = scheduler_create_task(this, dbd_taskwork);

  if (task == nullptr) {
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
}

/*-------------Taskwork*-------------*/
static DashboardDataStatus dbd_fetch_data(DashboardData *self,
                                          uint64_t now_ms) {
  if (!wifi_handler_is_connected()) {
    self->next_fetch_ms = now_ms + OPTIMAESTRO_FETCH_RETRY_MS;
    return DBD_STATE_IDLE;
  }

  if (self->need_weaterdata) {
    // TODO: replace with OptiMaestro weather/current endpoint when available.
  }

  if (!dbd_fetch_optimaestro_display_json(self)) {
    self->next_fetch_ms = now_ms + OPTIMAESTRO_FETCH_RETRY_MS;
    return DBD_STATE_IDLE;
  }

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
    self->state = dbd_fetch_data(self, _now_ms);
    break;

  case DBD_STATE_UPDATE_GRAPHS:
    self->state = dbd_update_graphs(self, _now_ms);
    break;

  default:
    break;
  }
}

/*-----------------------------------*/
