#ifndef __DASHBOARD_DATA_API_H__
#define __DASHBOARD_DATA_API_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  DASHBOARD_ENERGY_RANGE_24H = 0,
  DASHBOARD_ENERGY_RANGE_7D,
  DASHBOARD_ENERGY_RANGE_30D,
} DashboardEnergyRange;

void dashboard_data_request_energy_range(DashboardEnergyRange range);

#ifdef __cplusplus
}
#endif

#endif
