#ifndef __NTP_CLOCK_HPP__
#define __NTP_CLOCK_HPP__
#include "esp_sntp.h"
#include <time.h>

class NtpClock {
public:
  static void init() {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");

    esp_sntp_init();
  }

  static bool time_valid() {
    time_t now = 0;
    time(&now);

    return now > 100000;
  }

  static uint32_t epoch() {
    time_t now;
    time(&now);

    return static_cast<uint32_t>(now);
  }
};

#endif
