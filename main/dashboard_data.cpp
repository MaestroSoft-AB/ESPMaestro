#include "dashboard_data.hpp"
#include "cJSON.h"
#include "display_handler.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "facility_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "wifi_handler.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "DashboardData";
static DashboardData *g_dashboard_data_instance = nullptr;

#ifndef OPTIMAESTRO_DISPLAY_GRAPH_URL
#define OPTIMAESTRO_DISPLAY_GRAPH_URL                                          \
  "http://135.225.131.246:10580/api/v1/display/graph/hour"
#endif

#define OPTIMAESTRO_PROFILE_BUCKETS 96
#define OPTIMAESTRO_BUCKETS_PER_HOUR 4
#define OPTIMAESTRO_FETCH_INTERVAL_MS 90000
#define OPTIMAESTRO_FETCH_RETRY_MS 15000
#define OPTIMAESTRO_HTTP_TIMEOUT_MS 5000
#define OPTIMAESTRO_DISPLAY_HTTP_MAX_BODY 8192
#define OPEN_METEO_HTTP_MAX_BODY 12288
#define OPTIMAESTRO_HTTP_LOG_BODY_MAX 240
#define OPTIMAESTRO_FETCH_TASK_STACK 8192
#define OPTIMAESTRO_FETCH_TASK_PRIORITY 2
#define OPEN_METEO_FORECAST_URL_MAX 384
#define OPTIMAESTRO_WEATHER_URL_MAX 512

typedef struct {
  char *data;
  int len;
  int capacity;
} HttpBuffer;

typedef struct {
  bool success;
  DashboardEnergyRange range;
  float current_sek_kwh;
  float sek_24h[DASHBOARD_ENERGY_MAX_POINTS];
  uint32_t power_w;
  uint32_t max_power_w_24h;
  float current_kwh;
  float current_sek_h;
  uint32_t power_24h[DASHBOARD_ENERGY_MAX_POINTS];
  float kwh_24h[DASHBOARD_ENERGY_MAX_POINTS];
  float cost_24h[DASHBOARD_ENERGY_MAX_POINTS];
  uint8_t point_count;
  uint16_t interval_minutes;
  char labels[DASHBOARD_ENERGY_MAX_POINTS][6];
  bool has_data[DASHBOARD_ENERGY_MAX_POINTS];
  bool weather_success;
  float outdoor_c;
  char weather_summary[32];
  float temp_c_24h[24];
  uint8_t rain_percent_24h[24];
  uint16_t weather_code_24h[24];
  uint16_t shortwave_wm2_24h[24];
  float wind_kmh_24h[24];
  char weather_time_24h[24][6];
} DbdFetchResult;

/*------------------------------------*/
static void dbd_taskwork(void *_context, uint64_t _now);
static void dbd_fetch_worker_task(void *_context);
static const char *dbd_weather_code_summary(uint16_t code);
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

static void dbd_format_energy_label(time_t stamp, uint16_t interval_minutes,
                                    char out[6]) {
  if (out == NULL)
    return;

  struct tm tm = {};
  localtime_r(&stamp, &tm);

  if (interval_minutes >= 1440) {
    strftime(out, 6, "%m-%d", &tm);
  } else {
    strftime(out, 6, "%H:%M", &tm);
  }
}

static bool dbd_fetch_http_url(const char *url, char *response,
                               int response_capacity, const char *label) {
  if (url == NULL || response == NULL || response_capacity <= 0)
    return false;

  memset(response, 0, response_capacity);

  HttpBuffer buffer = {
      .data = response,
      .len = 0,
      .capacity = response_capacity,
  };

  esp_http_client_config_t config = {};
  config.url = url;
  config.timeout_ms = OPTIMAESTRO_HTTP_TIMEOUT_MS;
  config.event_handler = http_event_handler;
  config.user_data = &buffer;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == NULL)
    return false;

  esp_err_t err = esp_http_client_perform(client);
  int status = esp_http_client_get_status_code(client);

  if (err != ESP_OK || status != 200 || buffer.len == 0) {
    ESP_LOGW(TAG, "%s fetch failed: url=%s err=%s status=%d len=%d",
             label ? label : "HTTP", url, esp_err_to_name(err), status,
             buffer.len);
    if (buffer.len > 0) {
      int log_len = buffer.len < OPTIMAESTRO_HTTP_LOG_BODY_MAX
                        ? buffer.len
                        : OPTIMAESTRO_HTTP_LOG_BODY_MAX;
      ESP_LOGW(TAG, "%s response body: %.*s", label ? label : "HTTP", log_len,
               response);
    }
    esp_http_client_cleanup(client);
    return false;
  }

  esp_http_client_cleanup(client);
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
  if (!cJSON_IsString(version) || !cJSON_IsObject(series)) {
    cJSON_Delete(root);
    return false;
  }

  memset(result->sek_24h, 0, sizeof(result->sek_24h));
  memset(result->power_24h, 0, sizeof(result->power_24h));
  memset(result->kwh_24h, 0, sizeof(result->kwh_24h));
  memset(result->cost_24h, 0, sizeof(result->cost_24h));
  memset(result->labels, 0, sizeof(result->labels));
  memset(result->has_data, 0, sizeof(result->has_data));
  result->point_count = 0;
  result->interval_minutes = 60;
  result->current_sek_kwh = 0.0f;
  result->power_w = 0;
  result->max_power_w_24h = 0;
  result->current_kwh = 0.0f;
  result->current_sek_h = 0.0f;

  if (strcmp(version->valuestring, "od1") == 0) {
    cJSON *consumption = cJSON_GetObjectItemCaseSensitive(series, "c");
    cJSON *price = cJSON_GetObjectItemCaseSensitive(series, "p");

    if (!cJSON_IsArray(consumption) || !cJSON_IsArray(price) ||
        cJSON_GetArraySize(consumption) < OPTIMAESTRO_PROFILE_BUCKETS ||
        cJSON_GetArraySize(price) < OPTIMAESTRO_PROFILE_BUCKETS) {
      cJSON_Delete(root);
      return false;
    }

    time_t now = time(NULL);
    struct tm now_tm = {};
    localtime_r(&now, &now_tm);
    int current_hour = now_tm.tm_hour;
    if (current_hour < 0)
      current_hour = 0;
    if (current_hour > 23)
      current_hour = 23;
    int current_bucket = (current_hour * OPTIMAESTRO_BUCKETS_PER_HOUR) +
                         (now_tm.tm_min / 15);
    if (current_bucket < 0)
      current_bucket = 0;
    if (current_bucket >= OPTIMAESTRO_PROFILE_BUCKETS)
      current_bucket = OPTIMAESTRO_PROFILE_BUCKETS - 1;

    float total_kwh = 0.0f;
    float total_cost = 0.0f;
    uint32_t max_power = 0;
    float current_bucket_price_sek = 0.0f;

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

        if (index == current_bucket) {
          current_bucket_price_sek = (float)(bucket_msek / 1000.0);
        }
      }

      result->kwh_24h[hour] = (float)(hour_mwh / 1000.0);
      result->cost_24h[hour] = (float)hour_cost_sek;
      result->sek_24h[hour] =
          valid_buckets > 0 ? (float)((hour_price_msek / valid_buckets) / 1000.0)
                            : 0.0f;
      result->power_24h[hour] =
          (uint32_t)(result->kwh_24h[hour] * 1000.0f + 0.5f);
      result->has_data[hour] = true;
      snprintf(result->labels[hour], sizeof(result->labels[hour]), "%02d:00",
               hour);

      total_kwh += result->kwh_24h[hour];
      total_cost += result->cost_24h[hour];
      if (result->power_24h[hour] > max_power)
        max_power = result->power_24h[hour];
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
    result->point_count = 24;
    result->interval_minutes = 60;
    result->current_sek_kwh = current_bucket_price_sek;
    result->power_w = result->power_24h[current_hour];
    result->max_power_w_24h = max_power;
    result->current_kwh = total_kwh;
    result->current_sek_h = total_cost;
  } else if (strcmp(version->valuestring, "od2") == 0) {
    cJSON *consumption = cJSON_GetObjectItemCaseSensitive(series, "c");
    cJSON *price = cJSON_GetObjectItemCaseSensitive(series, "p");
    cJSON *cost = cJSON_GetObjectItemCaseSensitive(series, "o");
    cJSON *power = cJSON_GetObjectItemCaseSensitive(series, "x");
    cJSON *timestamps = cJSON_GetObjectItemCaseSensitive(series, "t");
    cJSON *available = cJSON_GetObjectItemCaseSensitive(series, "d");
    cJSON *minutes = cJSON_GetObjectItemCaseSensitive(root, "m");

    if (!cJSON_IsArray(consumption) || !cJSON_IsArray(price) ||
        !cJSON_IsArray(cost) || !cJSON_IsArray(power) ||
        !cJSON_IsArray(timestamps) || !cJSON_IsArray(available)) {
      cJSON_Delete(root);
      return false;
    }

    int point_count = cJSON_GetArraySize(consumption);
    if (point_count <= 0 || point_count > DASHBOARD_ENERGY_MAX_POINTS ||
        cJSON_GetArraySize(price) < point_count ||
        cJSON_GetArraySize(cost) < point_count ||
        cJSON_GetArraySize(power) < point_count ||
        cJSON_GetArraySize(timestamps) < point_count ||
        cJSON_GetArraySize(available) < point_count) {
      cJSON_Delete(root);
      return false;
    }

    result->point_count = (uint8_t)point_count;
    result->interval_minutes =
        cJSON_IsNumber(minutes) ? (uint16_t)minutes->valueint : 1440;

    for (int i = 0; i < point_count; i++) {
      double c_mwh = 0.0;
      double p_msek = 0.0;
      double o_cents = 0.0;
      double x_watts = 0.0;
      double ts = 0.0;
      double has = 0.0;

      if (!read_number_array_item(consumption, i, &c_mwh) ||
          !read_number_array_item(price, i, &p_msek) ||
          !read_number_array_item(cost, i, &o_cents) ||
          !read_number_array_item(power, i, &x_watts) ||
          !read_number_array_item(timestamps, i, &ts) ||
          !read_number_array_item(available, i, &has)) {
        cJSON_Delete(root);
        return false;
      }

      result->kwh_24h[i] = (float)(c_mwh / 1000.0);
      result->sek_24h[i] = (float)(p_msek / 1000.0);
      result->cost_24h[i] = (float)(o_cents / 100.0);
      result->power_24h[i] = (uint32_t)(x_watts < 0.0 ? 0.0 : x_watts);
      result->has_data[i] = has > 0.5;
      dbd_format_energy_label((time_t)ts, result->interval_minutes,
                              result->labels[i]);

      if (result->has_data[i]) {
        result->current_sek_kwh = result->sek_24h[i];
        result->power_w = result->power_24h[i];
        result->current_kwh += result->kwh_24h[i];
        result->current_sek_h += result->cost_24h[i];
        if (result->power_24h[i] > result->max_power_w_24h)
          result->max_power_w_24h = result->power_24h[i];
      }
    }

    if (cJSON_IsObject(summary)) {
      cJSON *ct = cJSON_GetObjectItemCaseSensitive(summary, "ct");
      cJSON *cc = cJSON_GetObjectItemCaseSensitive(summary, "cc");
      if (cJSON_IsNumber(ct))
        result->current_kwh = (float)(ct->valuedouble / 1000.0);
      if (cJSON_IsNumber(cc))
        result->current_sek_h = (float)(cc->valuedouble / 100.0);
    }

    result->success = true;
  } else {
    cJSON_Delete(root);
    return false;
  }

  cJSON_Delete(root);
  return true;
}

static bool dbd_fetch_optimaestro_display_json(DbdFetchResult *result,
                                               DashboardEnergyRange range) {
  if (result == NULL)
    return false;

  char *response = (char *)calloc(1, OPTIMAESTRO_DISPLAY_HTTP_MAX_BODY);
  if (response == NULL)
    return false;

  const char *range_param = "24h";
  if (range == DASHBOARD_ENERGY_RANGE_7D) {
    range_param = "7d";
  } else if (range == DASHBOARD_ENERGY_RANGE_30D) {
    range_param = "30d";
  }

  char url[256];
  snprintf(url, sizeof(url), "%s?range=%s", OPTIMAESTRO_DISPLAY_GRAPH_URL,
           range_param);

  if (!dbd_fetch_http_url(url, response,
                          OPTIMAESTRO_DISPLAY_HTTP_MAX_BODY,
                          "OptiMaestro display")) {
    free(response);
    return false;
  }

  bool parsed = dbd_parse_optimaestro_display_json(response, result);
  if (!parsed)
    ESP_LOGW(TAG, "Failed to parse OptiMaestro display data");

  free(response);
  return parsed;
}

static bool dbd_fetch_optimaestro_weather(DbdFetchResult *result) {
  (void)result;
  return false;
}

static const char *dbd_weather_code_summary(uint16_t code) {
  if (code == 0)
    return "Clear";
  if (code == 1 || code == 2)
    return "Partly cloudy";
  if (code == 3)
    return "Cloudy";
  if (code == 45 || code == 48)
    return "Fog";
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82))
    return "Rain";
  if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86))
    return "Snow";
  if (code >= 95)
    return "Thunder";
  return "Forecast";
}

static bool dbd_parse_open_meteo_json(const char *json,
                                      DbdFetchResult *result) {
  if (json == NULL || result == NULL)
    return false;

  cJSON *root = cJSON_Parse(json);
  if (root == NULL)
    return false;

  cJSON *hourly = cJSON_GetObjectItemCaseSensitive(root, "hourly");
  cJSON *times = cJSON_GetObjectItemCaseSensitive(hourly, "time");
  cJSON *temps = cJSON_GetObjectItemCaseSensitive(hourly, "temperature_2m");
  cJSON *rain =
      cJSON_GetObjectItemCaseSensitive(hourly, "precipitation_probability");
  cJSON *codes = cJSON_GetObjectItemCaseSensitive(hourly, "weather_code");
  cJSON *solar =
      cJSON_GetObjectItemCaseSensitive(hourly, "shortwave_radiation");
  cJSON *wind = cJSON_GetObjectItemCaseSensitive(hourly, "wind_speed_10m");

  if (!cJSON_IsArray(times) || !cJSON_IsArray(temps) || !cJSON_IsArray(rain) ||
      !cJSON_IsArray(codes) || !cJSON_IsArray(solar) || !cJSON_IsArray(wind) ||
      cJSON_GetArraySize(times) < 24 || cJSON_GetArraySize(temps) < 24 ||
      cJSON_GetArraySize(rain) < 24 || cJSON_GetArraySize(codes) < 24 ||
      cJSON_GetArraySize(solar) < 24 || cJSON_GetArraySize(wind) < 24) {
    cJSON_Delete(root);
    return false;
  }

  for (int i = 0; i < 24; i++) {
    double temp_v = 0.0;
    double rain_v = 0.0;
    double code_v = 0.0;
    double solar_v = 0.0;
    double wind_v = 0.0;
    cJSON *time_item = cJSON_GetArrayItem(times, i);

    if (!cJSON_IsString(time_item) ||
        !read_number_array_item(temps, i, &temp_v) ||
        !read_number_array_item(rain, i, &rain_v) ||
        !read_number_array_item(codes, i, &code_v) ||
        !read_number_array_item(solar, i, &solar_v) ||
        !read_number_array_item(wind, i, &wind_v)) {
      cJSON_Delete(root);
      return false;
    }

    result->temp_c_24h[i] = (float)temp_v;
    result->rain_percent_24h[i] = (uint8_t)(rain_v < 0.0     ? 0.0
                                            : rain_v > 100.0 ? 100.0
                                                             : rain_v);
    result->weather_code_24h[i] = (uint16_t)code_v;
    result->shortwave_wm2_24h[i] = (uint16_t)(solar_v < 0.0 ? 0.0 : solar_v);
    result->wind_kmh_24h[i] = (float)wind_v;

    const char *time_text = time_item->valuestring;
    if (strlen(time_text) >= 16) {
      snprintf(result->weather_time_24h[i], sizeof(result->weather_time_24h[i]),
               "%.5s", time_text + 11);
    } else {
      snprintf(result->weather_time_24h[i], sizeof(result->weather_time_24h[i]),
               "%02d:00", i);
    }
  }

  result->weather_success = true;
  result->outdoor_c = result->temp_c_24h[0];
  snprintf(result->weather_summary, sizeof(result->weather_summary), "%s",
           dbd_weather_code_summary(result->weather_code_24h[0]));

  cJSON_Delete(root);
  return true;
}

static bool dbd_fetch_open_meteo_forecast(DbdFetchResult *result) {
  if (result == NULL)
    return false;

  Facility_Config cfg = {};
  if (facility_config_load(&cfg) != ESP_OK || cfg.lat[0] == '\0' ||
      cfg.lon[0] == '\0') {
    ESP_LOGW(TAG, "Weather forecast skipped: facility coordinates missing");
    return false;
  }

  char url[OPEN_METEO_FORECAST_URL_MAX];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?"
           "latitude=%s&longitude=%s&"
           "hourly=temperature_2m,precipitation_probability,weather_code,"
           "shortwave_radiation,wind_speed_10m&forecast_days=1&timezone=auto",
           cfg.lat, cfg.lon);

  char *response = (char *)calloc(1, OPEN_METEO_HTTP_MAX_BODY);
  if (response == NULL)
    return false;

  if (!dbd_fetch_http_url(url, response, OPEN_METEO_HTTP_MAX_BODY,
                          "Open-Meteo")) {
    free(response);
    return false;
  }

  bool parsed = dbd_parse_open_meteo_json(response, result);
  if (!parsed)
    ESP_LOGW(TAG, "Failed to parse Open-Meteo forecast data");

  free(response);
  return parsed;
}

/*-----------------------------------*/

DashboardData::DashboardData()
    : initialized_(false), state(DBD_STATE_IDLE), need_weaterdata(true),
      need_electricitydata(true), need_realtimedata(true), task(nullptr),
      fetch_task(NULL), fetch_result_queue(NULL), fetch_in_progress(false),
      pending_range_refresh_(false), energy_range_(DASHBOARD_ENERGY_RANGE_24H),
      base_epoch(0), base_ms(0), next_fetch_ms(0) {
  g_dashboard_data_instance = this;
  fetch_result_queue = xQueueCreate(1, sizeof(DbdFetchResult));

  if (fetch_result_queue == NULL) {
    initialized_ = false;
    return;
  }

  BaseType_t task_created = xTaskCreate(
      dbd_fetch_worker_task, "dbd_http_fetch", OPTIMAESTRO_FETCH_TASK_STACK,
      this, OPTIMAESTRO_FETCH_TASK_PRIORITY, &fetch_task);
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

void DashboardData::update_weather_forecast(
    float outdoor_c, const char *summary, const float temp_c_24h[24],
    const uint8_t rain_percent_24h[24], const uint16_t weather_code_24h[24],
    const uint16_t shortwave_wm2_24h[24], const float wind_kmh_24h[24],
    const char time_24h[24][6]) {
  update_weather(outdoor_c, weatherdata_.indoor_c, summary);

  memcpy(weatherdata_.temp_c_24h, temp_c_24h, sizeof(weatherdata_.temp_c_24h));
  memcpy(weatherdata_.rain_percent_24h, rain_percent_24h,
         sizeof(weatherdata_.rain_percent_24h));
  memcpy(weatherdata_.weather_code_24h, weather_code_24h,
         sizeof(weatherdata_.weather_code_24h));
  memcpy(weatherdata_.shortwave_wm2_24h, shortwave_wm2_24h,
         sizeof(weatherdata_.shortwave_wm2_24h));
  memcpy(weatherdata_.wind_kmh_24h, wind_kmh_24h,
         sizeof(weatherdata_.wind_kmh_24h));
  memcpy(weatherdata_.time_24h, time_24h, sizeof(weatherdata_.time_24h));
}

void DashboardData::update_electricity(
    float current_sek_kwh, const float sek_24h[DASHBOARD_ENERGY_MAX_POINTS],
    uint8_t point_count, uint16_t interval_minutes,
    const char labels[DASHBOARD_ENERGY_MAX_POINTS][6],
    const bool has_data[DASHBOARD_ENERGY_MAX_POINTS]) {
  electricitydata_.valid = true;
  electricitydata_.current_sek_kwh = current_sek_kwh;
  memcpy(electricitydata_.sek_24h, sek_24h, sizeof(electricitydata_.sek_24h));
  electricitydata_.point_count = point_count;
  electricitydata_.interval_minutes = interval_minutes;
  memcpy(electricitydata_.labels, labels, sizeof(electricitydata_.labels));
  memcpy(electricitydata_.has_data, has_data, sizeof(electricitydata_.has_data));

  electricitydata_.updated_epoch = (uint32_t)time(NULL);
}

void DashboardData::update_realtime(uint32_t power_w, uint32_t max_power_w_24h,
                                    float current_kwh, float current_sek_h,
                                    const uint32_t power_24h[DASHBOARD_ENERGY_MAX_POINTS],
                                    const float kwh_24h[DASHBOARD_ENERGY_MAX_POINTS],
                                    const float cost_24h[DASHBOARD_ENERGY_MAX_POINTS],
                                    uint8_t point_count, uint16_t interval_minutes,
                                    const char labels[DASHBOARD_ENERGY_MAX_POINTS][6],
                                    const bool has_data[DASHBOARD_ENERGY_MAX_POINTS]) {
  realtimedata_.valid = true;
  realtimedata_.power_w = power_w;
  realtimedata_.max_power_w_24h = max_power_w_24h;
  realtimedata_.current_kwh = current_kwh;
  realtimedata_.current_sek_h = current_sek_h;
  memcpy(realtimedata_.power_24h, power_24h, sizeof(realtimedata_.power_24h));
  memcpy(realtimedata_.kwh_24h, kwh_24h, sizeof(realtimedata_.kwh_24h));
  memcpy(realtimedata_.cost_24h, cost_24h, sizeof(realtimedata_.cost_24h));
  realtimedata_.point_count = point_count;
  realtimedata_.interval_minutes = interval_minutes;
  memcpy(realtimedata_.labels, labels, sizeof(realtimedata_.labels));
  memcpy(realtimedata_.has_data, has_data, sizeof(realtimedata_.has_data));

  realtimedata_.updated_epoch = (uint32_t)time(NULL);
}

void DashboardData::request_energy_range(DashboardEnergyRange range) {
  if (energy_range_ == range && fetch_in_progress) {
    return;
  }

  if (fetch_in_progress && energy_range_ != range) {
    pending_range_refresh_ = true;
  }

  energy_range_ = range;
  next_fetch_ms = 0;
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
    DashboardEnergyRange range = self->energy_range_;
    result.range = range;
    result.success = dbd_fetch_optimaestro_display_json(&result, range);
    result.weather_success = dbd_fetch_optimaestro_weather(&result);
    if (!result.weather_success) {
      result.weather_success = dbd_fetch_open_meteo_forecast(&result);
    }

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
  bool stale_energy_result = result.range != self->energy_range_;
  bool pending_range_refresh = self->pending_range_refresh_;
  self->pending_range_refresh_ = false;

  if (!result.success && !result.weather_success) {
    if (pending_range_refresh || stale_energy_result) {
      self->next_fetch_ms = now_ms;
      return DBD_STATE_REQUEST_DATA;
    }
    self->next_fetch_ms = now_ms + OPTIMAESTRO_FETCH_RETRY_MS;
    return DBD_STATE_IDLE;
  }

  if (result.weather_success) {
    self->update_weather_forecast(
        result.outdoor_c, result.weather_summary, result.temp_c_24h,
        result.rain_percent_24h, result.weather_code_24h,
        result.shortwave_wm2_24h, result.wind_kmh_24h, result.weather_time_24h);
  }

  if (result.success && !stale_energy_result) {
    self->update_electricity(result.current_sek_kwh, result.sek_24h,
                             result.point_count, result.interval_minutes,
                             result.labels, result.has_data);
    self->update_realtime(result.power_w, result.max_power_w_24h,
                          result.current_kwh, result.current_sek_h,
                          result.power_24h, result.kwh_24h, result.cost_24h,
                          result.point_count, result.interval_minutes,
                          result.labels, result.has_data);
  }

  if (pending_range_refresh || stale_energy_result) {
    self->next_fetch_ms = now_ms;
    return DBD_STATE_REQUEST_DATA;
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

extern "C" void dashboard_data_request_energy_range(DashboardEnergyRange range) {
  if (g_dashboard_data_instance != nullptr) {
    g_dashboard_data_instance->request_energy_range(range);
  }
}

/*-----------------------------------*/
