#include "dashboard_data.hpp"
#include "cJSON.h"
#include "display_handler.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "facility_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "wifi_handler.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TAG = "DashboardData";
static DashboardData *g_dashboard_data_instance = nullptr;

#ifndef OPTIMAESTRO_DISPLAY_GRAPH_URL
#define OPTIMAESTRO_DISPLAY_GRAPH_URL                                          \
  "http://135.225.131.246:10580/api/v1/display/graph/hour"
#endif

#ifndef OPTIMAESTRO_WEATHER_CACHE_URL
#define OPTIMAESTRO_WEATHER_CACHE_URL                                          \
  "http://135.225.131.246:10580/api/v1/weather/cache"
#endif

#define OPTIMAESTRO_PROFILE_BUCKETS 96
#define OPTIMAESTRO_BUCKETS_PER_HOUR 4
#define OPTIMAESTRO_FETCH_INTERVAL_MS 90000
#define OPTIMAESTRO_FETCH_RETRY_MS 15000
#define OPTIMAESTRO_REFRESH_PERIOD_SEC 900
#define OPTIMAESTRO_REFRESH_OFFSET_SEC 30
#define OPTIMAESTRO_HTTP_TIMEOUT_MS 5000
#define OPTIMAESTRO_DISPLAY_HTTP_MAX_BODY 8192
#define OPTIMAESTRO_WEATHER_HTTP_MAX_BODY 32768
#define OPTIMAESTRO_HTTP_LOG_BODY_MAX 240
#define OPTIMAESTRO_FETCH_TASK_STACK 8192
#define OPTIMAESTRO_FETCH_TASK_PRIORITY 2
#define OPTIMAESTRO_WEATHER_URL_MAX 512

typedef struct {
  char *data;
  int len;
  int capacity;
} HttpBuffer;

typedef struct {
  float temp_sum;
  float precip_sum;
  float wind_sum;
  float wind_max;
  float solar_sum;
  uint32_t count;
} WeatherBucketAccum;

typedef struct {
  bool success;
  DashboardEnergyRange range;
  float current_sek_kwh;
  float avg_sek_kwh_day;
  float sek_24h[DASHBOARD_ENERGY_MAX_POINTS];
  uint32_t power_w;
  uint32_t historical_avg_power_w;
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
  float temp_c_24h[DASHBOARD_WEATHER_HOURLY_POINTS];
  uint8_t rain_percent_24h[DASHBOARD_WEATHER_HOURLY_POINTS];
  uint16_t weather_code_24h[DASHBOARD_WEATHER_HOURLY_POINTS];
  uint16_t shortwave_wm2_24h[DASHBOARD_WEATHER_HOURLY_POINTS];
  float wind_kmh_24h[DASHBOARD_WEATHER_HOURLY_POINTS];
  char weather_time_24h[DASHBOARD_WEATHER_HOURLY_POINTS][6];
  float temp_c_daily[DASHBOARD_WEATHER_DAILY_POINTS];
  uint8_t rain_percent_daily[DASHBOARD_WEATHER_DAILY_POINTS];
  uint16_t weather_code_daily[DASHBOARD_WEATHER_DAILY_POINTS];
  float wind_kmh_daily[DASHBOARD_WEATHER_DAILY_POINTS];
  char weather_time_daily[DASHBOARD_WEATHER_DAILY_POINTS][6];
  uint8_t daily_count;
} DbdFetchResult;

/*------------------------------------*/
static void dbd_taskwork(void *_context, uint64_t _now);
static void dbd_fetch_worker_task(void *_context);
static const char *dbd_weather_code_summary(uint16_t code);
static bool dbd_wall_clock_ready(time_t epoch_now);
static uint64_t dbd_next_aligned_fetch_ms(uint64_t now_ms);
static void dbd_url_encode_query_value(const char *in, char *out,
                                       size_t out_size);
static uint8_t dbd_precipitation_to_percent(float mm);
static uint16_t dbd_infer_weather_code(float precipitation_mm, float wind_kmh,
                                       float shortwave_wm2);
static void dbd_format_weather_label(time_t stamp, char out[6]);
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

static bool read_object_number_item(cJSON *object, const char *key, double *out) {
  if (object == NULL || key == NULL || out == NULL)
    return false;

  cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
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

static void dbd_format_weather_label(time_t stamp, char out[6]) {
  if (out == NULL)
    return;

  struct tm tm = {};
  localtime_r(&stamp, &tm);
  strftime(out, 6, "%H:%M", &tm);
}

static bool dbd_wall_clock_ready(time_t epoch_now) {
  return epoch_now >= 1704067200; // 2024-01-01 00:00:00 UTC
}

static uint64_t dbd_next_aligned_fetch_ms(uint64_t now_ms) {
  time_t epoch_now = time(NULL);
  if (!dbd_wall_clock_ready(epoch_now)) {
    return now_ms + OPTIMAESTRO_FETCH_INTERVAL_MS;
  }

  time_t next_epoch =
      (((epoch_now / OPTIMAESTRO_REFRESH_PERIOD_SEC) + 1) *
       OPTIMAESTRO_REFRESH_PERIOD_SEC) +
      OPTIMAESTRO_REFRESH_OFFSET_SEC;

  int64_t delta_ms = ((int64_t)next_epoch - (int64_t)epoch_now) * 1000LL;
  if (delta_ms < 1000) {
    delta_ms = 1000;
  }

  return now_ms + (uint64_t)delta_ms;
}

static void dbd_url_encode_query_value(const char *in, char *out,
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
      out[used++] = hex[ch & 0x0F];
    } else {
      break;
    }
  }

  out[used] = '\0';
}

static uint8_t dbd_precipitation_to_percent(float mm) {
  if (mm <= 0.0f)
    return 0;
  if (mm < 0.1f)
    return 10;
  if (mm < 0.3f)
    return 25;
  if (mm < 0.7f)
    return 40;
  if (mm < 1.5f)
    return 60;
  if (mm < 3.0f)
    return 80;
  return 100;
}

static uint16_t dbd_infer_weather_code(float precipitation_mm, float wind_kmh,
                                       float shortwave_wm2) {
  if (precipitation_mm >= 1.0f)
    return 61;
  if (precipitation_mm > 0.0f)
    return 51;
  if (shortwave_wm2 >= 400.0f)
    return 0;
  if (shortwave_wm2 >= 180.0f)
    return 2;
  if (wind_kmh >= 14.0f)
    return 3;
  return 3;
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
  result->avg_sek_kwh_day = 0.0f;
  result->power_w = 0;
  result->historical_avg_power_w = 0;
  result->max_power_w_24h = 0;
  result->current_kwh = 0.0f;
  result->current_sek_h = 0.0f;

  if (strcmp(version->valuestring, "od1") == 0) {
    cJSON *consumption = cJSON_GetObjectItemCaseSensitive(series, "c");
    cJSON *history = cJSON_GetObjectItemCaseSensitive(series, "h");
    cJSON *price = cJSON_GetObjectItemCaseSensitive(series, "p");

    if (!cJSON_IsArray(consumption) || !cJSON_IsArray(history) ||
        !cJSON_IsArray(price) ||
        cJSON_GetArraySize(consumption) < OPTIMAESTRO_PROFILE_BUCKETS ||
        cJSON_GetArraySize(history) < OPTIMAESTRO_PROFILE_BUCKETS ||
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
    int current_bucket =
        (current_hour * OPTIMAESTRO_BUCKETS_PER_HOUR) + (now_tm.tm_min / 15);
    if (current_bucket < 0)
      current_bucket = 0;
    if (current_bucket >= OPTIMAESTRO_PROFILE_BUCKETS)
      current_bucket = OPTIMAESTRO_PROFILE_BUCKETS - 1;

    float total_kwh = 0.0f;
    float total_cost = 0.0f;
    uint32_t max_power = 0;
    float current_bucket_price_sek = 0.0f;
    double total_price_msek = 0.0;

    for (int hour = 0; hour < 24; hour++) {
      double hour_mwh = 0.0;
      double hour_cost_sek = 0.0;
      double hour_price_msek = 0.0;
      int valid_buckets = 0;

      for (int bucket = 0; bucket < OPTIMAESTRO_BUCKETS_PER_HOUR; bucket++) {
        int index = (hour * OPTIMAESTRO_BUCKETS_PER_HOUR) + bucket;
        double bucket_mwh = 0.0;
        double history_bucket_mwh = 0.0;
        double bucket_msek = 0.0;

        if (!read_number_array_item(consumption, index, &bucket_mwh) ||
            !read_number_array_item(history, index, &history_bucket_mwh) ||
            !read_number_array_item(price, index, &bucket_msek)) {
          cJSON_Delete(root);
          return false;
        }

        hour_mwh += bucket_mwh;
        hour_price_msek += bucket_msek;
        total_price_msek += bucket_msek;
        hour_cost_sek += (bucket_mwh / 1000.0) * (bucket_msek / 1000.0);
        valid_buckets++;

        if (index == current_bucket) {
          current_bucket_price_sek = (float)(bucket_msek / 1000.0);
          result->historical_avg_power_w =
              (uint32_t)(((history_bucket_mwh / 1000.0) * 4000.0) + 0.5);
        }
      }

      result->kwh_24h[hour] = (float)(hour_mwh / 1000.0);
      result->cost_24h[hour] = (float)hour_cost_sek;
      result->sek_24h[hour] =
          valid_buckets > 0
              ? (float)((hour_price_msek / valid_buckets) / 1000.0)
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
    result->avg_sek_kwh_day =
        (float)((total_price_msek / OPTIMAESTRO_PROFILE_BUCKETS) / 1000.0);
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

    int valid_price_points = 0;
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
        result->avg_sek_kwh_day += result->sek_24h[i];
        valid_price_points++;
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

    if (valid_price_points > 0) {
      result->avg_sek_kwh_day /= (float)valid_price_points;
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

  if (!dbd_fetch_http_url(url, response, OPTIMAESTRO_DISPLAY_HTTP_MAX_BODY,
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

static bool dbd_parse_optimaestro_weather_json(const char *json,
                                               DbdFetchResult *result) {
  if (json == NULL || result == NULL)
    return false;

  cJSON *root = cJSON_Parse(json);
  if (root == NULL)
    return false;

  cJSON *meta = cJSON_GetObjectItemCaseSensitive(root, "meta");
  cJSON *values = cJSON_GetObjectItemCaseSensitive(root, "values");
  cJSON *interval_minutes_item =
      cJSON_GetObjectItemCaseSensitive(meta, "interval_minutes");

  if (!cJSON_IsArray(values) || cJSON_GetArraySize(values) <= 0) {
    cJSON_Delete(root);
    return false;
  }

  memset(result->temp_c_24h, 0, sizeof(result->temp_c_24h));
  memset(result->rain_percent_24h, 0, sizeof(result->rain_percent_24h));
  memset(result->weather_code_24h, 0, sizeof(result->weather_code_24h));
  memset(result->shortwave_wm2_24h, 0, sizeof(result->shortwave_wm2_24h));
  memset(result->wind_kmh_24h, 0, sizeof(result->wind_kmh_24h));
  memset(result->weather_time_24h, 0, sizeof(result->weather_time_24h));
  memset(result->temp_c_daily, 0, sizeof(result->temp_c_daily));
  memset(result->rain_percent_daily, 0, sizeof(result->rain_percent_daily));
  memset(result->weather_code_daily, 0, sizeof(result->weather_code_daily));
  memset(result->wind_kmh_daily, 0, sizeof(result->wind_kmh_daily));
  memset(result->weather_time_daily, 0, sizeof(result->weather_time_daily));
  result->daily_count = 0;

  time_t now = time(NULL);
  struct tm now_tm = {};
  localtime_r(&now, &now_tm);
  struct tm hour_tm = now_tm;
  hour_tm.tm_min = 0;
  hour_tm.tm_sec = 0;
  time_t hour_start = mktime(&hour_tm);
  struct tm day_tm = now_tm;
  day_tm.tm_hour = 0;
  day_tm.tm_min = 0;
  day_tm.tm_sec = 0;
  time_t day_start = mktime(&day_tm);

  (void)interval_minutes_item;

  WeatherBucketAccum hourly[DASHBOARD_WEATHER_HOURLY_POINTS] = {};
  WeatherBucketAccum daily[DASHBOARD_WEATHER_DAILY_POINTS] = {};
  float daily_temp_max[DASHBOARD_WEATHER_DAILY_POINTS];
  bool daily_has_data[DASHBOARD_WEATHER_DAILY_POINTS] = {};
  memset(daily_temp_max, 0, sizeof(daily_temp_max));

  bool current_found = false;
  time_t current_ts = 0;
  float current_temp = 0.0f;
  float current_precip = 0.0f;
  float current_wind = 0.0f;
  float current_solar = 0.0f;

  int value_count = cJSON_GetArraySize(values);
  for (int i = 0; i < value_count; i++) {
    cJSON *item = cJSON_GetArrayItem(values, i);
    double ts_v = 0.0;
    double temp_v = 0.0;
    double precip_v = 0.0;
    double wind_v = 0.0;
    double solar_v = 0.0;

    if (!cJSON_IsObject(item) ||
        !read_object_number_item(item, "timestamp", &ts_v) ||
        !read_object_number_item(item, "temperature", &temp_v) ||
        !read_object_number_item(item, "precipitation", &precip_v) ||
        !read_object_number_item(item, "windspeed", &wind_v) ||
        !read_object_number_item(item, "radiation_shortwave", &solar_v)) {
      continue;
    }

    time_t ts = (time_t)ts_v;
    float temp = (float)temp_v;
    float precip = (float)(precip_v < 0.0 ? 0.0 : precip_v);
    float wind = (float)(wind_v < 0.0 ? 0.0 : wind_v);
    float solar = (float)(solar_v < 0.0 ? 0.0 : solar_v);

    if (ts <= now) {
      if (!current_found || ts >= current_ts) {
        current_found = true;
        current_ts = ts;
        current_temp = temp;
        current_precip = precip;
        current_wind = wind;
        current_solar = solar;
      }
    }

    if (ts >= hour_start &&
        ts < hour_start + (DASHBOARD_WEATHER_HOURLY_POINTS * 3600)) {
      int hour_index = (int)((ts - hour_start) / 3600);
      if (hour_index >= 0 && hour_index < DASHBOARD_WEATHER_HOURLY_POINTS) {
        hourly[hour_index].temp_sum += temp;
        hourly[hour_index].precip_sum += precip;
        hourly[hour_index].wind_sum += wind;
        if (wind > hourly[hour_index].wind_max)
          hourly[hour_index].wind_max = wind;
        hourly[hour_index].solar_sum += solar;
        hourly[hour_index].count++;
      }
    }

    if (ts >= day_start &&
        ts < day_start + ((time_t)DASHBOARD_WEATHER_DAILY_POINTS * 86400)) {
      int day_index = (int)((ts - day_start) / 86400);
      if (day_index >= 0 && day_index < DASHBOARD_WEATHER_DAILY_POINTS) {
        daily[day_index].temp_sum += temp;
        daily[day_index].precip_sum += precip;
        daily[day_index].wind_sum += wind;
        if (wind > daily[day_index].wind_max)
          daily[day_index].wind_max = wind;
        daily[day_index].solar_sum += solar;
        daily[day_index].count++;
        if (!daily_has_data[day_index] || temp > daily_temp_max[day_index]) {
          daily_temp_max[day_index] = temp;
        }
        daily_has_data[day_index] = true;
      }
    }
  }

  int hourly_valid = 0;
  float last_temp = current_found ? current_temp : 0.0f;
  uint8_t last_rain = dbd_precipitation_to_percent(current_precip);
  uint16_t last_code =
      dbd_infer_weather_code(current_precip, current_wind, current_solar);
  uint16_t last_solar = (uint16_t)(current_solar < 0.0f ? 0.0f : current_solar);
  float last_wind = current_wind;

  for (int i = 0; i < DASHBOARD_WEATHER_HOURLY_POINTS; i++) {
    time_t label_ts = hour_start + ((time_t)i * 3600);
    dbd_format_weather_label(label_ts, result->weather_time_24h[i]);

    if (hourly[i].count > 0) {
      float avg_temp = hourly[i].temp_sum / (float)hourly[i].count;
      float avg_wind = hourly[i].wind_sum / (float)hourly[i].count;
      float avg_solar = hourly[i].solar_sum / (float)hourly[i].count;
      uint8_t rain_percent = dbd_precipitation_to_percent(hourly[i].precip_sum);
      uint16_t weather_code = dbd_infer_weather_code(
          hourly[i].precip_sum, hourly[i].wind_max, avg_solar);

      result->temp_c_24h[i] = avg_temp;
      result->rain_percent_24h[i] = rain_percent;
      result->weather_code_24h[i] = weather_code;
      result->shortwave_wm2_24h[i] =
          (uint16_t)(avg_solar < 0.0f ? 0.0f : avg_solar);
      result->wind_kmh_24h[i] = avg_wind;

      last_temp = avg_temp;
      last_rain = rain_percent;
      last_code = weather_code;
      last_solar = result->shortwave_wm2_24h[i];
      last_wind = avg_wind;
      hourly_valid++;
    } else {
      result->temp_c_24h[i] = last_temp;
      result->rain_percent_24h[i] = last_rain;
      result->weather_code_24h[i] = last_code;
      result->shortwave_wm2_24h[i] = last_solar;
      result->wind_kmh_24h[i] = last_wind;
    }
  }

  for (int i = 0; i < DASHBOARD_WEATHER_DAILY_POINTS; i++) {
    if (!daily_has_data[i] || daily[i].count == 0)
      continue;

    struct tm label_tm = day_tm;
    label_tm.tm_mday += i;
    time_t label_ts = mktime(&label_tm);

    result->temp_c_daily[i] = daily_temp_max[i];
    result->rain_percent_daily[i] =
        dbd_precipitation_to_percent(daily[i].precip_sum);
    result->weather_code_daily[i] = dbd_infer_weather_code(
        daily[i].precip_sum, daily[i].wind_max,
        daily[i].solar_sum / (float)daily[i].count);
    result->wind_kmh_daily[i] = daily[i].wind_max;
    struct tm label_tm_local = {};
    localtime_r(&label_ts, &label_tm_local);
    strftime(result->weather_time_daily[i],
             sizeof(result->weather_time_daily[i]), "%m-%d",
             &label_tm_local);
    result->daily_count = (uint8_t)(i + 1);
  }

  if (!current_found && hourly_valid > 0) {
    current_temp = result->temp_c_24h[0];
    current_precip = hourly[0].precip_sum;
    current_wind = result->wind_kmh_24h[0];
    current_solar = result->shortwave_wm2_24h[0];
    current_found = true;
  }

  if (!current_found || hourly_valid == 0) {
    cJSON_Delete(root);
    return false;
  }

  result->weather_success = true;
  result->outdoor_c = current_temp;
  snprintf(result->weather_summary, sizeof(result->weather_summary), "%s",
           dbd_weather_code_summary(
               dbd_infer_weather_code(current_precip, current_wind, current_solar)));

  cJSON_Delete(root);
  return true;
}

static bool dbd_fetch_optimaestro_weather(DbdFetchResult *result) {
  if (result == NULL)
    return false;

  Facility_Config cfg = {};
  if (facility_config_load(&cfg) != ESP_OK || cfg.facility_name[0] == '\0') {
    ESP_LOGW(TAG, "Weather fetch skipped: facility configuration missing");
    return false;
  }

  time_t now = time(NULL);
  struct tm now_tm = {};
  localtime_r(&now, &now_tm);
  now_tm.tm_min = 0;
  now_tm.tm_sec = 0;
  time_t hour_start = mktime(&now_tm);
  struct tm day_tm = now_tm;
  day_tm.tm_hour = 0;
  time_t day_start = mktime(&day_tm);
  time_t end = day_start + ((time_t)DASHBOARD_WEATHER_DAILY_POINTS * 86400);

  char encoded_name[96];
  dbd_url_encode_query_value(cfg.facility_name, encoded_name,
                             sizeof(encoded_name));

  char url[OPTIMAESTRO_WEATHER_URL_MAX];
  snprintf(url, sizeof(url),
           "%s?forecast=1&from=%lld&to=%lld&name=%s",
           OPTIMAESTRO_WEATHER_CACHE_URL, (long long)hour_start,
           (long long)end, encoded_name);

  char *response = (char *)calloc(1, OPTIMAESTRO_WEATHER_HTTP_MAX_BODY);
  if (response == NULL)
    return false;

  if (!dbd_fetch_http_url(url, response, OPTIMAESTRO_WEATHER_HTTP_MAX_BODY,
                          "OptiMaestro weather")) {
    free(response);
    return false;
  }

  bool parsed = dbd_parse_optimaestro_weather_json(response, result);
  if (!parsed)
    ESP_LOGW(TAG, "OptiMaestro weather cache not ready or parse failed");

  free(response);
  return parsed;
}

/*-----------------------------------*/

DashboardData::DashboardData()
    : initialized_(false), state(DBD_STATE_IDLE), need_weatherdata(true),
      need_electricitydata(true), need_realtimedata(true), task(nullptr),
      fetch_task(NULL), fetch_result_queue(NULL), fetch_in_progress(false),
      pending_range_refresh_(false), pending_manual_refresh_(false),
      energy_range_(DASHBOARD_ENERGY_RANGE_24H), manual_refresh_ms_(0),
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
    float outdoor_c, const char *summary,
    const float temp_c_24h[DASHBOARD_WEATHER_HOURLY_POINTS],
    const uint8_t rain_percent_24h[DASHBOARD_WEATHER_HOURLY_POINTS],
    const uint16_t weather_code_24h[DASHBOARD_WEATHER_HOURLY_POINTS],
    const uint16_t shortwave_wm2_24h[DASHBOARD_WEATHER_HOURLY_POINTS],
    const float wind_kmh_24h[DASHBOARD_WEATHER_HOURLY_POINTS],
    const char time_24h[DASHBOARD_WEATHER_HOURLY_POINTS][6],
    const float temp_c_daily[DASHBOARD_WEATHER_DAILY_POINTS],
    const uint8_t rain_percent_daily[DASHBOARD_WEATHER_DAILY_POINTS],
    const uint16_t weather_code_daily[DASHBOARD_WEATHER_DAILY_POINTS],
    const float wind_kmh_daily[DASHBOARD_WEATHER_DAILY_POINTS],
    const char time_daily[DASHBOARD_WEATHER_DAILY_POINTS][6],
    uint8_t daily_count) {
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
  memcpy(weatherdata_.temp_c_daily, temp_c_daily,
         sizeof(weatherdata_.temp_c_daily));
  memcpy(weatherdata_.rain_percent_daily, rain_percent_daily,
         sizeof(weatherdata_.rain_percent_daily));
  memcpy(weatherdata_.weather_code_daily, weather_code_daily,
         sizeof(weatherdata_.weather_code_daily));
  memcpy(weatherdata_.wind_kmh_daily, wind_kmh_daily,
         sizeof(weatherdata_.wind_kmh_daily));
  memcpy(weatherdata_.time_daily, time_daily, sizeof(weatherdata_.time_daily));
  weatherdata_.daily_count = daily_count;
}

void DashboardData::update_electricity(
    float current_sek_kwh, float avg_sek_kwh_day,
    const float sek_24h[DASHBOARD_ENERGY_MAX_POINTS],
    uint8_t point_count, uint16_t interval_minutes,
    const char labels[DASHBOARD_ENERGY_MAX_POINTS][6],
    const bool has_data[DASHBOARD_ENERGY_MAX_POINTS]) {
  electricitydata_.valid = true;
  electricitydata_.current_sek_kwh = current_sek_kwh;
  electricitydata_.avg_sek_kwh_day = avg_sek_kwh_day;
  memcpy(electricitydata_.sek_24h, sek_24h, sizeof(electricitydata_.sek_24h));
  electricitydata_.point_count = point_count;
  electricitydata_.interval_minutes = interval_minutes;
  memcpy(electricitydata_.labels, labels, sizeof(electricitydata_.labels));
  memcpy(electricitydata_.has_data, has_data,
         sizeof(electricitydata_.has_data));

  electricitydata_.updated_epoch = (uint32_t)time(NULL);
}

void DashboardData::update_realtime(
    uint32_t power_w, uint32_t historical_avg_power_w,
    uint32_t max_power_w_24h, float current_kwh, float current_sek_h,
    const uint32_t power_24h[DASHBOARD_ENERGY_MAX_POINTS],
    const float kwh_24h[DASHBOARD_ENERGY_MAX_POINTS],
    const float cost_24h[DASHBOARD_ENERGY_MAX_POINTS], uint8_t point_count,
    uint16_t interval_minutes,
    const char labels[DASHBOARD_ENERGY_MAX_POINTS][6],
    const bool has_data[DASHBOARD_ENERGY_MAX_POINTS]) {
  realtimedata_.valid = true;
  realtimedata_.power_w = power_w;
  realtimedata_.historical_avg_power_w = historical_avg_power_w;
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

void DashboardData::request_refresh(uint32_t delay_ms) {
  uint64_t now_ms = (uint64_t)(esp_timer_get_time() / 1000ULL);
  uint64_t target_ms = now_ms + (uint64_t)delay_ms;

  if (fetch_in_progress) {
    if (!pending_manual_refresh_ || target_ms < manual_refresh_ms_) {
      manual_refresh_ms_ = target_ms;
    }
    pending_manual_refresh_ = true;
    return;
  }

  pending_manual_refresh_ = false;
  manual_refresh_ms_ = 0;
  if (next_fetch_ms == 0 || target_ms < next_fetch_ms) {
    next_fetch_ms = target_ms;
  }
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

  if (self->need_weatherdata) {
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
  bool pending_manual_refresh = self->pending_manual_refresh_;
  uint64_t manual_refresh_ms = self->manual_refresh_ms_;
  self->pending_range_refresh_ = false;

  if (!result.success && !result.weather_success) {
    if (pending_range_refresh || stale_energy_result) {
      self->next_fetch_ms = now_ms;
      return DBD_STATE_REQUEST_DATA;
    }
    if (pending_manual_refresh) {
      if (manual_refresh_ms <= now_ms) {
        self->pending_manual_refresh_ = false;
        self->manual_refresh_ms_ = 0;
        self->next_fetch_ms = now_ms;
        return DBD_STATE_REQUEST_DATA;
      }
      self->next_fetch_ms = manual_refresh_ms;
      return DBD_STATE_IDLE;
    }
    self->next_fetch_ms = now_ms + OPTIMAESTRO_FETCH_RETRY_MS;
    return DBD_STATE_IDLE;
  }

  if (result.weather_success) {
    self->update_weather_forecast(
        result.outdoor_c, result.weather_summary, result.temp_c_24h,
        result.rain_percent_24h, result.weather_code_24h,
        result.shortwave_wm2_24h, result.wind_kmh_24h, result.weather_time_24h,
        result.temp_c_daily, result.rain_percent_daily,
        result.weather_code_daily, result.wind_kmh_daily,
        result.weather_time_daily, result.daily_count);
  }

  if (result.success && !stale_energy_result) {
    self->update_electricity(result.current_sek_kwh, result.avg_sek_kwh_day,
                             result.sek_24h,
                             result.point_count, result.interval_minutes,
                             result.labels, result.has_data);
    self->update_realtime(result.power_w, result.historical_avg_power_w,
                          result.max_power_w_24h,
                          result.current_kwh, result.current_sek_h,
                          result.power_24h, result.kwh_24h, result.cost_24h,
                          result.point_count, result.interval_minutes,
                          result.labels, result.has_data);
  }

  if (pending_range_refresh || stale_energy_result) {
    self->next_fetch_ms = now_ms;
    return DBD_STATE_REQUEST_DATA;
  }

  if (pending_manual_refresh) {
    if (manual_refresh_ms <= now_ms) {
      self->pending_manual_refresh_ = false;
      self->manual_refresh_ms_ = 0;
      self->next_fetch_ms = now_ms;
      return DBD_STATE_REQUEST_DATA;
    }

    uint64_t aligned_fetch_ms = dbd_next_aligned_fetch_ms(now_ms);
    self->next_fetch_ms = manual_refresh_ms < aligned_fetch_ms
                              ? manual_refresh_ms
                              : aligned_fetch_ms;
    return DBD_STATE_UPDATE_GRAPHS;
  }

  self->next_fetch_ms = dbd_next_aligned_fetch_ms(now_ms);
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

extern "C" void
dashboard_data_request_energy_range(DashboardEnergyRange range) {
  if (g_dashboard_data_instance != nullptr) {
    g_dashboard_data_instance->request_energy_range(range);
  }
}

extern "C" void dashboard_data_request_refresh(uint32_t delay_ms) {
  if (g_dashboard_data_instance != nullptr) {
    g_dashboard_data_instance->request_refresh(delay_ms);
  }
}

/*-----------------------------------*/
