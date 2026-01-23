#ifndef UI_TOGGLE_H
#define UI_TOGGLE_H
#include "lvgl.h"
#include "ui_styles.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_toggle_callback_t)(bool state);
typedef struct {
    const char *label;
    bool initial_state;
    ui_toggle_callback_t callback;
} ui_toggle_config_t;

lv_obj_t* ui_toggle_create(lv_obj_t *parent, const ui_toggle_config_t *config);
void ui_toggle_set_state(lv_obj_t *toggle, bool state);
bool ui_toggle_get_state(lv_obj_t *toggle);

#ifdef __cplusplus
}
#endif
#endif
