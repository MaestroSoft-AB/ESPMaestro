#ifndef __ESPM_DISPLAY_HANDLER_H__
#define __ESPM_DISPLAY_HANDLER_H__

#include "wifi_handler.h"
#include <stdbool.h>
#include <stdlib.h>
#define DISPLAY_SIZE_WIDTH 1024
#define DISPLAY_SIZE_HEIGHT 600
/* Lazy calc on 14px mono font on 1024x600 display */
#define DISPLAY_MAX_CHAR_PER_ROW 73
#define DISPLAY_MAX_CHAR_ROWS 42

typedef struct {

} DH;

typedef struct {
  bool scan_ready;
  char scan_options[1024];

  bool status_ready;
  bool connected;
  char ssid[33];
  char ip[16];
  char message[64];
} DH_wifi_status;

/*-----------Callbacks-----*/
void on_wifi_status(bool _connected, const char *_ssid, const char *_ip,
                    const char *_message);
void on_wifi_scan_done(const Wifi_Handler_ap *_aps, uint16_t _count);

/* ======================= INTERFACE ======================= */

int display_handler_init(DH *_DH);

void display_handler_work(void *_null_for_now);
void display_handler_wifi_status(bool connected, const char *ssid,
                                 const char *ip);

// void dh_dispose(DH* _DH);

/* ========================================================= */
#endif
