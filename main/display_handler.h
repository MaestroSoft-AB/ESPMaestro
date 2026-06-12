#ifndef __ESPM_DISPLAY_HANDLER_H__
#define __ESPM_DISPLAY_HANDLER_H__

#include "dashboard_types.h"
#include "wifi_handler.h"
#include <stdbool.h>
#include <stdlib.h>
/** @brief Display width in pixels. */
#define DISPLAY_SIZE_WIDTH 1024

/** @brief Display height in pixels. */

/* Lazy calc on 14px mono font on 1024x600 display */
#define DISPLAY_SIZE_HEIGHT 600

#include "i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Display handler configuration and runtime context.
 *
 * Contains hardware resources required by the display handler. The I2C port is
 * used by the GT911 touch controller and must be initialized before calling
 * display_handler_init().
 */
typedef struct {
  /** @brief Shared I2C port used by display-related peripherals. */
  DEV_I2C_Port i2c;
  SemaphoreHandle_t i2c_mutex;
} DH;

/**
 * @brief Buffered Wi-Fi status data used by the display handler.
 *
 * This structure is used internally to transfer Wi-Fi scan and connection
 * updates from Wi-Fi callbacks to the LVGL display task.
 */
typedef struct {
  /** @brief True when a Wi-Fi scan result is ready for UI processing. */
  bool scan_ready;

  /** @brief Newline-separated list of scanned Wi-Fi network SSIDs. */
  char scan_options[1024];

  /** @brief True when a Wi-Fi connection status update is ready. */
  bool status_ready;

  /** @brief True if Wi-Fi is currently connected. */
  bool connected;

  /** @brief Connected Wi-Fi SSID. */
  char ssid[33];

  /** @brief Current IPv4 address string. */
  char ip[16];

  /** @brief Status or error message shown in the Wi-Fi setup UI. */
  char message[64];
} DH_wifi_status;

/**
 * @brief Buffered time update used by the display handler.
 */
typedef struct {
  /** @brief True when a new time value is ready for UI processing. */
  bool time_ready;

  /** @brief Hour value. */
  uint8_t h;

  /** @brief Minute value. */
  uint8_t m;

  /** @brief Second value. */
  uint8_t s;
} DH_time_status;

/**
 * @brief Buffered date update used by the display handler.
 */
typedef struct {
  /** @brief True when a new date value is ready for UI processing. */
  bool date_ready;

  /** @brief Year value. */
  uint16_t year;

  /** @brief Month value in the range 1-12. */
  uint8_t month;

  /** @brief Day of month value in the range 1-31. */
  uint8_t day;
} DH_date_status;

/**
 * @brief Buffered BME280 environmental reading used by the display handler.
 */
typedef struct {
  bool has_indoor_climate;
  bool indoor_climate_ready;
  float temperature_c;
  float pressure_hpa;
  float humidity_rh;
} DH_indoor_climate_status;

/*-----------Callbacks-----*/

/**
 * @brief Wi-Fi connection status callback.
 *
 * Stores the latest Wi-Fi connection state, SSID, IP address, and status
 * message so that the display task can update the UI safely from its own
 * context.
 *
 * @param _connected True if Wi-Fi is connected.
 * @param _ssid Connected SSID, or null if unavailable.
 * @param _ip IPv4 address string, or null if unavailable.
 * @param _message Optional status or error message.
 */
void on_wifi_status(bool _connected, const char *_ssid, const char *_ip,
                    const char *_message);

/**
 * @brief Wi-Fi scan completion callback.
 *
 * Converts scanned access points into a newline-separated list of SSIDs and
 * stores it for later processing by the display task.
 *
 * @param _aps Array of scanned Wi-Fi access points.
 * @param _count Number of entries in @p _aps.
 */
void on_wifi_scan_done(const Wifi_Handler_ap *_aps, uint16_t _count);

/* ======================= INTERFACE ======================= */

/**
 * @brief Initialize the display handler.
 *
 * Initializes the GT911 touch controller using the provided shared I2C bus,
 * initializes the RGB LCD panel, starts the LVGL port, enables the backlight,
 * and creates the mutexes used for thread-safe UI update buffering.
 *
 * @param _DH Display handler configuration containing an initialized I2C bus.
 * @return 0 on success, -1 on initialization failure.
 */
int display_handler_init(DH *_DH);

/**
 * @brief Run the display handler work loop.
 *
 * Initializes the UI and then continuously processes buffered updates for
 * Wi-Fi status, time, date, dashboard data, setup wizard state, footer text,
 * and BME280 readings. This function does not return and is intended to run
 * inside its own FreeRTOS task.
 *
 * @param _null_for_now Unused task parameter.
 */
void display_handler_work(void *_null_for_now);

/**
 * @brief Immediately update the Wi-Fi status area in the UI.
 *
 * This function locks the LVGL port and directly updates the UI with the
 * supplied Wi-Fi connection state.
 *
 * @param connected True if Wi-Fi is connected.
 * @param ssid Connected SSID, or null if unavailable.
 * @param ip IPv4 address string, or null if unavailable.
 */
void display_handler_wifi_status(bool connected, const char *ssid,
                                 const char *ip);

/**
 * @brief Queue a time update for the display task.
 *
 * The values are stored in a protected buffer and later applied by
 * display_handler_work().
 *
 * @param h Hour value.
 * @param m Minute value.
 * @param s Second value.
 */
void display_handler_update_time(uint8_t h, uint8_t m, uint8_t s);

/**
 * @brief Queue a date update for the display task.
 *
 * The values are stored in a protected buffer and later applied by
 * display_handler_work().
 *
 * @param year Full year value.
 * @param month Month value in the range 1-12.
 * @param day Day of month value in the range 1-31.
 */
void display_handler_update_date(uint16_t year, uint8_t month, uint8_t day);
void display_handler_update_live_power(uint32_t power_w);
/**
 * @brief Queue dashboard data for display.
 *
 * Copies the provided weather, electricity, and realtime data structures into
 * internal buffers. Null pointers are ignored individually.
 *
 * @param w Weather data, or null to leave the previous value unchanged.
 * @param e Electricity price data, or null to leave the previous value
 * unchanged.
 * @param r Realtime consumption data, or null to leave the previous value
 * unchanged.
 */
void display_handler_update_dashboard(const WeatherData *w,
                                      const ElectricityData *e,
                                      const RealtimeData *r);

/**
 * @brief Queue a BME280 reading for display.
 *
 * Stores temperature, humidity, and pressure values for later UI processing.
 *
 * @param temp Temperature in degrees Celsius.
 * @param humidity Relative humidity in percent RH.
 * @param hpa Atmospheric pressure in hectopascals.
 */
void display_handler_update_bme280(float temp, float humidity, float hpa);

/**
 * @brief Request that the setup wizard is shown.
 *
 * The setup wizard is shown when at least one required setup item is missing.
 *
 * @param missing_wifi True if Wi-Fi setup is missing.
 * @param missing_facility True if facility configuration is missing.
 */
void display_handler_start_setup_wizard(bool missing_wifi,
                                        bool missing_facility);

/**
 * @brief Queue footer text for display.
 *
 * Copies the supplied text into an internal buffer and applies it from the
 * display task.
 *
 * @param text Footer text to display. Null is ignored.
 */
void display_handler_set_footer_text(const char *text);

/* ========================================================= */
#ifdef __cplusplus
}
#endif
#endif
