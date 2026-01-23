#include "ui_button.h"
#include <string.h>
#include <stdio.h>

static void button_event_cb(lv_event_t *e) {
    ui_button_callback_t callback = (ui_button_callback_t)lv_event_get_user_data(e);
    if (callback != NULL) callback();
}

lv_obj_t* ui_button_create(lv_obj_t *parent, const ui_button_config_t *config) {
    if (!parent || !config) return NULL;
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, config->full_width ? LV_PCT(100) : LV_SIZE_CONTENT, UI_BUTTON_HEIGHT);
    lv_obj_set_style_bg_color(btn, config->bg_color, 0);
    lv_obj_set_style_bg_color(btn, UI_COLOR_BG_SELECTED, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_BORDER, 0);
    lv_obj_set_style_radius(btn, UI_RADIUS_MEDIUM, 0);
    lv_obj_set_style_pad_all(btn, UI_PADDING_MEDIUM, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    
    lv_obj_t *label = lv_label_create(btn);
    if (config->icon && config->label) {
        char text[128];
        snprintf(text, sizeof(text), "%s %s", config->icon, config->label);
        lv_label_set_text(label, text);
    } else lv_label_set_text(label, config->icon ? config->icon : config->label);
    lv_obj_set_style_text_color(label, config->text_color, 0);
    lv_obj_set_style_text_font(label, UI_FONT_MEDIUM, 0);
    lv_obj_center(label);
    
    if (config->callback) lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, (void*)config->callback);
    return btn;
}

lv_obj_t* ui_button_create_back(lv_obj_t *parent, ui_button_callback_t callback) {
    ui_button_config_t config = {
        .label = "Back", .icon = LV_SYMBOL_LEFT, .callback = callback,
        .bg_color = UI_COLOR_BG_SECONDARY, .text_color = UI_COLOR_TEXT_PRIMARY, .full_width = false,
    };
    return ui_button_create(parent, &config);
}
