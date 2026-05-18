#ifndef __WIFI_HANDLER_H__
#define __WIFI_HANDLER_H__

#include "esp_err.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include <stdbool.h>
#include <stdint.h>
/**
 * @file wifi_handler.h
 * @brief WiFi manager for ESP32-S3.
 *
 * Provides functionality for scanning networks, connecting/disconnecting,
 * handling auto-reconnect, and storing WiFi credentials using ESP-IDF (NVS).
 */

/**
 * @brief Maximum number of access points returned during scan.
 */
#define WIFI_MANAGER_MAX_APS 20

/**
 * @brief Maximum number of reconnect attempts before entering ERROR state.
 */
#define WIFI_MAX_RETRIES 2

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Internal state machine for WiFi handler.
 */
typedef enum {
  WIFI_HANDLER_STATE_IDLE = 0,       /**< No active WiFi operation */
  WIFI_HANDLER_STATE_SCANNING,       /**< Scanning for networks */
  WIFI_HANDLER_STATE_CONNECTING,     /**< Attempting to connect */
  WIFI_HANDLER_STATE_CONNECTED,      /**< Connected and IP acquired */
  WIFI_HANDLER_STATE_RECONNECT_WAIT, /**< Waiting before reconnect attempt */
  WIFI_HANDLER_STATE_SWITCHING, /**< Switching after connecting to a new network
                                 */
  WIFI_HANDLER_STATE_ERROR,     /**< Error state after failure */
} wifi_handler_state;

/**
 * @brief Represents a scanned WiFi access point.
 */
typedef struct {
  char ssid[33]; /**< SSID (null-terminated string) */
  int8_t rssi;   /**< Signal strength (RSSI) */
  bool open;     /**< True if network is open (no password) */
} Wifi_Handler_ap;

/**
 * @brief Callback triggered when a WiFi scan completes.
 *
 * @param _aps   Array of detected access points
 * @param _count Number of valid entries in the array
 */
typedef void (*wifi_handler_scan_cb)(const Wifi_Handler_ap *_aps,
                                     uint16_t _count);

/**
 * @brief Callback triggered on WiFi status updates.
 *
 * @param _connected True if connected, false otherwise
 * @param _ssid      Current SSID (if available)
 * @param _ip        Assigned IP address (if connected)
 * @param _message   Human-readable status message
 */
typedef void (*wifi_handler_status_cb)(bool _connected, const char *_ssid,
                                       const char *_ip, const char *_message);

/**
 * @brief Internal WiFi handler state container.
 *
 * This struct holds runtime state and configuration for the WiFi subsystem.
 * It is used internally as a singleton instance.
 */
typedef struct {
  wifi_handler_state state;         /**< Current state */
  wifi_handler_scan_cb scan_cb;     /**< Scan result callback */
  wifi_handler_status_cb status_cb; /**< Status update callback */
  esp_netif_t *sta_netif;           /**< Station network interface */
  bool user_connect;                /**< True if connect button was clicked*/
  bool reconnect_after_scan; /**< True if connection to new network failed*/
  bool user_disconnect;      /**< True if disconnect was user-triggered */
  char current_ssid[33];     /**< Currently configured SSID */
  int retry_count;           /**< Number of reconnect attempts */
  wifi_config_t cfg;
} Wifi_Handler;

/**
 * @brief Initialize the WiFi handler.
 *
 * Sets up WiFi, event loop, and registers callbacks.
 * Automatically attempts to reconnect if saved credentials exist.
 *
 * @param _scan_cb   Callback for scan results
 * @param _status_cb Callback for status updates
 *
 * @return ESP_OK on success, otherwise error code
 */
esp_err_t wifi_handler_init(wifi_handler_scan_cb _scan_cb,
                            wifi_handler_status_cb _status_cb);

/**
 * @brief Start scanning for WiFi networks.
 *
 * If currently connected or connecting, WiFi will be temporarily disconnected.
 *
 * @return ESP_OK if scan started successfully, otherwise error
 */
esp_err_t wifi_handler_scan(void);

/**
 * @brief Connect to a WiFi network.
 *
 * Stores credentials in NVS and initiates connection.
 *
 * @param _ssid     SSID to connect to
 * @param _password Password (can be NULL for open networks)
 *
 * @return ESP_OK on success, otherwise error
 */
esp_err_t wifi_handler_connect(const char *_ssid, const char *_password);

/**
 * @brief Disconnect from the current WiFi network.
 *
 * Marks the disconnect as user-initiated and stops auto-reconnect.
 *
 * @return ESP_OK on success, otherwise error
 */
esp_err_t wifi_handler_disconnect(void);

/**
 * @brief Check if WiFi is currently connected.
 *
 * @return true if connected, false otherwise
 */
bool wifi_handler_is_connected(void);
#ifdef __cplusplus
}
#endif
#endif
