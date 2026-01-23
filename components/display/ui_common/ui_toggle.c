#include "ui_toggle.h"

static void toggle_event_cb(lv_event_t *e) {
    ui_toggle_callback_t callback = (ui_toggle_callback_t)lv_event_get_user_data(e);
    if (callback) callback(lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
}

lv_obj_t* ui_toggle_create(lv_obj_t *parent, const ui_toggle_config_t *config) {
    if (!parent || !config) return NULL;
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, LV_PCT(100), UI_LIST_ITEM_HEIGHT);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, UI_PADDING_MEDIUM, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *label = lv_label_create(container);
    lv_label_set_text(label, config->label);
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(label, UI_FONT_MEDIUM, 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    
    lv_obj_t *toggle = lv_switch_create(container);
    lv_obj_set_size(toggle, UI_TOGGLE_WIDTH, UI_TOGGLE_HEIGHT);
    lv_obj_align(toggle, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(toggle, UI_COLOR_TEXT_SECONDARY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(toggle, UI_COLOR_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
    
    if (config->initial_state) lv_obj_add_state(toggle, LV_STATE_CHECKED);
    if (config->callback) lv_obj_add_event_cb(toggle, toggle_event_cb, LV_EVENT_VALUE_CHANGED, (void*)config->callback);
    return container;
}

void ui_toggle_set_state(lv_obj_t *container, bool state) {
    if (!container) return;
    lv_obj_t *toggle = lv_obj_get_child(container, -1);
    state ? lv_obj_add_state(toggle, LV_STATE_CHECKED) : lv_obj_clear_state(toggle, LV_STATE_CHECKED);
}

bool ui_toggle_get_state(lv_obj_t *container) {
    if (!container) return false;
    return lv_obj_has_state(lv_obj_get_child(container, -1), LV_STATE_CHECKED);
}
