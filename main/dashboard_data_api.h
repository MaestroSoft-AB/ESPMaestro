#ifndef __DASHBOARD_DATA_API_H__
#define __DASHBOARD_DATA_API_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  DASHBOARD_ENERGY_RANGE_24H = 0,
  DASHBOARD_ENERGY_RANGE_7D,
  DASHBOARD_ENERGY_RANGE_30D,
} DashboardEnergyRange;

void dashboard_data_request_energy_range(DashboardEnergyRange range);
void dashboard_data_request_refresh(uint32_t delay_ms);

#ifdef __cplusplus
}
#endif

#endif
