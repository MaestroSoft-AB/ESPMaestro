#include "dashboard_data.hpp"
#include "display_handler.h"
#include "esp_log.h"

static const char *TAG = "DashboardData";
/*------------------------------------*/
static void dbd_taskwork(void *_context, uint64_t _now);
/*------------------------------------*/
static void dbd_make_mock_data(DashboardData *self) {
  float sek_24h[24];
  uint32_t power_24h[24];
  float kwh_24h[24];
  float cost_24h[24];

  float total_kwh = 0.0f;
  float total_cost = 0.0f;
  uint32_t max_power = 0;

  for (int i = 0; i < 24; i++) {
    // Simple daily curve: low at night, high in the morning/evening.
    uint32_t p = 250 + ((i >= 6 && i <= 9) ? 900 : 0) +
                 ((i >= 17 && i <= 21) ? 1400 : 0) + ((i * 73) % 350);

    float kwh = p / 1000.0f; // About one hour of consumption.
    float price = 0.65f + ((i * 11) % 60) / 100.0f;
    float cost = kwh * price;

    power_24h[i] = p;
    kwh_24h[i] = kwh;
    sek_24h[i] = price;
    cost_24h[i] = cost;

    total_kwh += kwh;
    total_cost += cost;
    if (p > max_power)
      max_power = p;
  }

  self->update_weather(7.5f, 21.2f, "Cloudy");
  self->update_electricity(sek_24h[23], sek_24h);
  self->update_realtime(power_24h[23], max_power, total_kwh, total_cost,
                        power_24h, kwh_24h, cost_24h);
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

  if (self->need_electricitydata) {
    // fetch electricity_data
  }

  if (self->need_realtimedata) {
    // fetch realtime_data
  }

  if (self->need_weaterdata) {
    // fetch weatherdata
  }
  // Save json to stack
  dbd_make_mock_data(self);
  self->next_fetch_ms = now_ms + 90000;
  return DBD_STATE_UPDATE_GRAPHS;
  // Change this when real data is available return DBD_STATE_PARSE_DATA;
}

static DashboardDataStatus dbd_parse_data(DashboardData *self,
                                          uint64_t now_ms) {

  // PARSE JSON SHIT HERE

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

  case DBD_STATE_PARSE_DATA:
    self->state = dbd_parse_data(self, _now_ms);
    break;

  case DBD_STATE_UPDATE_GRAPHS:
    self->state = dbd_update_graphs(self, _now_ms);
    break;

  default:
    break;
  }
}

/*-----------------------------------*/
