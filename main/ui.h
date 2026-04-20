#ifndef __UI_H_
#define __UI_H_
#include "lvgl.h"
#include <stdbool.h>

typedef enum {
  UI_SCREEN_HOME = 0,
  UI_SCREEN_SETTINGS,
  UI_SCREEN_WIFI,
  UI_SCREEN_FACILITY,
} UI_Screen;

typedef struct {
  lv_obj_t *root;
  lv_obj_t *header;
  lv_obj_t *footer;
  lv_obj_t *content;

  lv_obj_t *header_title;
  lv_obj_t *footer_label;
  lv_obj_t *back_btn;
  lv_obj_t *settings_btn;

  /*----------Screns--------*/
  lv_obj_t *screen_home;
  lv_obj_t *screen_settings;
  lv_obj_t *screen_wifi;
  lv_obj_t *screen_facility;

  /*----Setting screen cards------*/
  lv_obj_t *card_wifi;
  lv_obj_t *card_facility;

  /*-----------Wifi widgets--------*/
  lv_obj_t *wifi_ssid_ta;
  lv_obj_t *wifi_pass_ta;
  lv_obj_t *wifi_status_label;
  lv_obj_t *wifi_connect_btn;
  lv_obj_t *wifi_spinner;

  lv_obj_t *wifi_scan_btn;
  lv_obj_t *wifi_networks_dd;
  lv_obj_t *wifi_keyboard;

  /*-----------Facility widgets--------*/
  lv_obj_t *facility_name_ta;
  lv_obj_t *facility_country_ta;
  lv_obj_t *facility_address_ta;
  lv_obj_t *facility_city_ta;
  lv_obj_t *facility_save_btn;
  lv_obj_t *facility_status_label;

  /*---------NAV------------*/
  UI_Screen current_screen;

  /*----------State--------*/
  bool wifi_connecting;
  bool wifi_connected;
  char wifi_status[128];

} UI;

void ui_init(UI *_UI);
void ui_show_home_screen(UI *_UI);
void ui_show_screen(UI *_UI, UI_Screen _screen);
void ui_set_wifi_form_status(UI *_UI, const char *_msg, bool _error);
void ui_set_wifi_busy(UI *_UI, bool _busy);

void ui_tick(UI *_UI);
void ui_set_footer_text(UI *_UI, const char *_text);
void ui_set_wifi_status(UI *_UI, bool _connected, const char *_ssid,
                        const char *_ip);
void ui_set_wifi_network_list(UI *_UI, const char *_options);

#endif
