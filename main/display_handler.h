#ifndef __ESPM_DISPLAY_HANDLER_H__
#define __ESPM_DISPLAY_HANDLER_H__

#include "dashboard_types.h"
#include "wifi_handler.h"
#include <stdbool.h>
#include <stdlib.h>

/** @brief Display width in pixels. */
#define DISPLAY_SIZE_WIDTH 1024

/** @brief Display height in pixels. */
#define DISPLAY_SIZE_HEIGHT 600

#include "i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Display handler configuration and runtime context.
 *
 * Must be populated by the caller and passed to display_handler_init().
 * Both the I2C port and mutex must be fully initialized before that call;
 * they are used by the GT911 touch controller and by any other peripheral
 * that shares the same I2C bus.
 */
typedef struct {
  /** @brief Shared I2C port used by display-related peripherals (GT911). */
  DEV_I2C_Port i2c;

  /**
   * @brief Mutex that guards all I2C transactions on the shared bus.
   *
   * Must be a valid FreeRTOS mutex created before display_handler_init() is
   * called. The display handler stores the handle in its global state and uses
   * it for every I2C access after init.
   */
  SemaphoreHandle_t i2c_mutex;
} DH;

/**
 * @brief Buffered Wi-Fi status data used by the display handler.
 *
 * Written by on_wifi_status() and on_wifi_scan_done() from Wi-Fi callbacks,
 * and consumed by display_handler_work() in the LVGL task. Access is guarded
 * by an internal mutex.
 */
typedef struct {
  /** @brief True when a completed Wi-Fi scan result is waiting to be shown. */
  bool scan_ready;

  /**
   * @brief Newline-separated list of scanned SSIDs.
   *
   * Populated by on_wifi_scan_done(). Set to "No networks found" when the
   * scan returns no usable results.
   */
  char scan_options[1024];

  /** @brief True when a Wi-Fi connection status update is waiting to be
   * shown. */
  bool status_ready;

  /** @brief True if Wi-Fi is currently connected. */
  bool connected;

  /** @brief SSID of the connected network (null-terminated, max 32 chars). */
  char ssid[33];

  /** @brief Current IPv4 address as a dotted-decimal string (max 15 chars). */
  char ip[16];

  /**
   * @brief Status or error message shown in the Wi-Fi setup UI.
   *
   * Populated from the @p _message argument of on_wifi_status(). May be an
   * empty string if no message was provided.
   */
  char message[64];
} DH_wifi_status;

/**
 * @brief Buffered time update used by the display handler.
 *
 * Written by display_handler_update_time() and consumed by
 * display_handler_work(). Access is guarded by an internal mutex.
 */
typedef struct {
  /** @brief True when new time values are waiting to be applied to the UI. */
  bool time_ready;

  /** @brief Hour value (0–23). */
  uint8_t h;

  /** @brief Minute value (0–59). */
  uint8_t m;

  /** @brief Second value (0–59). */
  uint8_t s;
} DH_time_status;

/**
 * @brief Buffered date update used by the display handler.
 *
 * Written by display_handler_update_date() and consumed by
 * display_handler_work(). Access is guarded by an internal mutex.
 */
typedef struct {
  /** @brief True when a new date value is waiting to be applied to the UI. */
  bool date_ready;

  /** @brief Full four-digit year (e.g. 2025). */
  uint16_t year;

  /** @brief Month value in the range 1–12. */
  uint8_t month;

  /** @brief Day-of-month value in the range 1–31. */
  uint8_t day;
} DH_date_status;

/**
 * @brief Buffered BME280 indoor climate reading used by the display handler.
 *
 * Written by display_handler_update_bme280() and consumed by
 * display_handler_work(). Updates are suppressed when all three values are
 * within 0.1 of the previously stored reading. Access is guarded by an
 * internal mutex.
 */
typedef struct {
  /**
   * @brief True after the first valid reading has been stored.
   *
   * Used to force an initial UI update even if no previous value exists for
   * comparison.
   */
  bool has_indoor_climate;

  /** @brief True when new climate values are waiting to be applied to the UI.
   */
  bool indoor_climate_ready;

  /** @brief Temperature in degrees Celsius. */
  float temperature_c;

  /** @brief Atmospheric pressure in hectopascals. */
  float pressure_hpa;

  /** @brief Relative humidity in percent RH. */
  float humidity_rh;
} DH_indoor_climate_status;

/* ======================= CALLBACKS ======================= */

/**
 * @brief Wi-Fi connection status callback.
 *
 * Intended to be registered with the Wi-Fi handler and called from a Wi-Fi
 * event context. Stores the connection state, SSID, IP address, and status
 * message in a mutex-protected buffer so that display_handler_work() can
 * update the UI safely from its own context.
 *
 * Null string pointers are accepted and stored as empty strings.
 *
 * @param _connected True if Wi-Fi is now connected.
 * @param _ssid      Connected network SSID, or null if unavailable.
 * @param _ip        IPv4 address string, or null if unavailable.
 * @param _message   Optional status or error message, or null for no message.
 */
void on_wifi_status(bool _connected, const char *_ssid, const char *_ip,
                    const char *_message);

/**
 * @brief Wi-Fi scan completion callback.
 *
 * Intended to be registered with the Wi-Fi handler and called when a scan
 * finishes. Builds a newline-separated list of non-empty SSIDs from @p _aps
 * and stores it in a mutex-protected buffer. If no usable SSIDs are found,
 * the buffer is set to "No networks found". The buffer is consumed by
 * display_handler_work().
 *
 * @param _aps   Array of scanned access point descriptors.
 * @param _count Number of entries in @p _aps.
 */
void on_wifi_scan_done(const Wifi_Handler_ap *_aps, uint16_t _count);

/* ======================= INTERFACE ======================= */

/**
 * @brief Initialize the display handler.
 *
 * Performs the following steps in order:
 * 1. Initializes the GT911 touch controller via the shared I2C bus, including
 *    IO expander reset sequencing and address configuration.
 * 2. Initializes the RGB LCD panel.
 * 3. Enables the LCD backlight via the IO expander.
 * 4. Starts the LVGL port with the panel and touch handles.
 * 5. Creates all internal mutexes used for thread-safe update buffering
 *    (Wi-Fi status, time, date, indoor climate, live power, dashboard, setup
 *    wizard, and footer text).
 *
 * This function must be called before any other display_handler_* function.
 * The LVGL UI is not initialized here; call display_handler_work() from a
 * dedicated FreeRTOS task to complete UI setup and process updates.
 *
 * @param _DH Display handler configuration with an initialized I2C bus and
 *            mutex.
 * @return 0  on success.
 * @return -1 if @p _DH is null, the I2C bus or mutex is missing, touch or
 *            panel initialization fails, the LVGL port cannot start, or any
 *            mutex cannot be created.
 */
int display_handler_init(DH *_DH);

/**
 * @brief Run the display handler work loop.
 *
 * Initializes the LVGL UI, then enters an infinite loop that wakes every
 * 150 ms and processes all pending buffered updates in the following order:
 * Wi-Fi status/scan, time, date, indoor climate, live power, dashboard data,
 * setup wizard, and footer text. Each update is applied under the LVGL port
 * lock.
 *
 * This function never returns and must be called from a dedicated FreeRTOS
 * task. display_handler_init() must have succeeded before calling this.
 *
 * @param _null_for_now Unused task parameter; pass NULL.
 */
void display_handler_work(void *_null_for_now);

/**
 * @brief Directly update the Wi-Fi status area in the UI.
 *
 * Unlike the buffered update path, this function acquires the LVGL port lock
 * and calls ui_set_wifi_status() immediately from the calling context. Prefer
 * this when the caller is already running at a safe priority and does not need
 * to defer the update.
 *
 * @param connected True if Wi-Fi is connected.
 * @param ssid      Connected SSID string, or null to clear the field.
 * @param ip        IPv4 address string, or null to clear the field.
 */
void display_handler_wifi_status(bool connected, const char *ssid,
                                 const char *ip);

/**
 * @brief Queue a time update for the display task.
 *
 * Stores the supplied values in a mutex-protected buffer and sets a ready
 * flag. The values are applied to the UI by the next display_handler_work()
 * iteration.
 *
 * @param h Hour value (0–23).
 * @param m Minute value (0–59).
 * @param s Second value (0–59).
 */
void display_handler_update_time(uint8_t h, uint8_t m, uint8_t s);

/**
 * @brief Queue a date update for the display task.
 *
 * Stores the supplied values in a mutex-protected buffer and sets a ready
 * flag. The values are applied to the UI by the next display_handler_work()
 * iteration.
 *
 * @param year  Full four-digit year (e.g. 2025).
 * @param month Month value in the range 1–12.
 * @param day   Day-of-month value in the range 1–31.
 */
void display_handler_update_date(uint16_t year, uint8_t month, uint8_t day);

/**
 * @brief Queue a live power reading for the display task.
 *
 * Stores @p power_w in a mutex-protected buffer. Updates are suppressed when
 * the value equals the previously buffered reading. The value is applied to
 * the UI by the next display_handler_work() iteration.
 *
 * @param power_w Current power consumption in watts.
 */
void display_handler_update_live_power(uint32_t power_w);

/**
 * @brief Queue dashboard data for the display task.
 *
 * Copies any non-null data structures into mutex-protected internal buffers
 * and sets a ready flag. All three datasets are applied together by the next
 * display_handler_work() iteration; passing null for an individual pointer
 * leaves the previously buffered value for that dataset unchanged.
 *
 * @param w Weather data to display, or null to leave unchanged.
 * @param e Electricity price data to display, or null to leave unchanged.
 * @param r Realtime consumption data to display, or null to leave unchanged.
 */
void display_handler_update_dashboard(const WeatherData *w,
                                      const ElectricityData *e,
                                      const RealtimeData *r);

/**
 * @brief Queue a BME280 indoor climate reading for the display task.
 *
 * Stores temperature, humidity, and pressure in a mutex-protected buffer.
 * Updates where all three values are within 0.1 of the current buffer are
 * silently discarded to avoid unnecessary UI redraws. The values are applied
 * to the UI by the next display_handler_work() iteration.
 *
 * @param temp     Temperature in degrees Celsius.
 * @param humidity Relative humidity in percent RH.
 * @param hpa      Atmospheric pressure in hectopascals.
 */
void display_handler_update_bme280(float temp, float humidity, float hpa);

/**
 * @brief Queue a setup wizard activation for the display task.
 *
 * Stores the missing-item flags in a mutex-protected buffer. The setup wizard
 * is shown by the next display_handler_work() iteration if either flag is
 * true. If both flags are false the ready flag is not set and no wizard is
 * shown.
 *
 * @param missing_wifi     True if Wi-Fi credentials have not been configured.
 * @param missing_facility True if facility configuration is incomplete.
 */
void display_handler_start_setup_wizard(bool missing_wifi,
                                        bool missing_facility);

/**
 * @brief Queue footer text for the display task.
 *
 * Copies @p text into a 128-byte mutex-protected buffer (truncated if
 * necessary) and sets a ready flag. The text is applied to the UI by the
 * next display_handler_work() iteration. Null pointers are silently ignored.
 *
 * @param text Null-terminated footer string to display.
 */
void display_handler_set_footer_text(const char *text);

/* ========================================================= */
#ifdef __cplusplus
}
#endif
#endif
