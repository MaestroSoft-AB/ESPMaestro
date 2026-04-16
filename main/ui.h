#ifndef __UI_H_
#define __UI_H_
#include "lvgl.h"
#include <stdbool.h>

typedef struct {
  lv_obj_t *root;
  lv_obj_t *header;
  lv_obj_t *body;
  lv_obj_t *footer;

  lv_obj_t *header_title;
  lv_obj_t *body_label;
  lv_obj_t *footer_label;

  lv_obj_t *header_btn_row;
  lv_obj_t *btn_wifi;
  lv_obj_t *btn_menu;

  char wifi_status[128];

} UI;

void ui_init(UI *_UI);
void ui_set_body_text(UI *_UI, const char *_text);
void ui_set_footer_text(UI *_UI, const char *_text);
void ui_set_wifi_status(UI *_UI, bool _connected, const char *_ssid,
                        const char *_ip);
void ui_tick(UI *_UI);

#endif
