/**
 * @file ui_button.h
 * @brief Custom button component for Kraken OS
 */

#ifndef UI_BUTTON_H
#define UI_BUTTON_H

#include "lvgl.h"
#include "ui_styles.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_button_callback_t)(void);

typedef struct {
    const char *label;
    const char *icon;
    ui_button_callback_t callback;
    lv_color_t bg_color;
    lv_color_t text_color;
    bool full_width;
} ui_button_config_t;

lv_obj_t* ui_button_create(lv_obj_t *parent, const ui_button_config_t *config);
lv_obj_t* ui_button_create_back(lv_obj_t *parent, ui_button_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // UI_BUTTON_H
