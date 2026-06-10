#ifndef __NTP_CLOCK_HPP__
#define __NTP_CLOCK_HPP__
#include "esp_sntp.h"
#include <time.h>

/**
 * @brief Simple static wrapper around ESP-IDF SNTP functionality.
 *
 * Provides initialization of the SNTP client and helper functions for checking
 * whether system time has been synchronized and for retrieving the current
 * Unix epoch timestamp.
 *
 * All methods are static and the class is not intended to be instantiated.
 */
class NtpClock {
public:
  /**
   * @brief Initialize the SNTP client.
   *
   * Configures SNTP in polling mode using the public NTP server
   * "pool.ntp.org" and starts the ESP-IDF SNTP service.
   *
   * This function should typically be called once during system startup.
   */
  static void init() {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");

    esp_sntp_init();
  }

  /**
   * @brief Check whether the system clock appears to be synchronized.
   *
   * The implementation considers the clock valid when the current Unix
   * timestamp is greater than 100000, indicating that SNTP has likely
   * synchronized the system time.
   *
   * @return true if the system time appears valid, false otherwise.
   */
  static bool time_valid() {
    time_t now = 0;
    time(&now);

    return now > 100000;
  }

  /**
   * @brief Get the current Unix epoch timestamp.
   *
   * @return Current time in seconds since the Unix epoch (1970-01-01 UTC).
   */
  static uint32_t epoch() {
    time_t now;
    time(&now);

    return static_cast<uint32_t>(now);
  }
};

#endif
