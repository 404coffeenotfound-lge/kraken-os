#ifndef UI_KEYBOARD_H
#define UI_KEYBOARD_H
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_keyboard_callback_t)(const char *text, bool cancelled);
lv_obj_t* ui_keyboard_show(lv_obj_t *parent, const char *title, const char *placeholder, ui_keyboard_callback_t callback);
void ui_keyboard_hide(lv_obj_t *keyboard);

#ifdef __cplusplus
}
#endif
#endif
