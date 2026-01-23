#include "ui_keyboard.h"
#include "ui_styles.h"
#include "ui_button.h"
#include <string.h>

typedef struct {
    lv_obj_t *container;
    lv_obj_t *textarea;
    lv_obj_t *keyboard;
    ui_keyboard_callback_t callback;
} ui_keyboard_ctx_t;

static void keyboard_btn_event_cb(lv_event_t *e) {
    ui_keyboard_ctx_t *ctx = (ui_keyboard_ctx_t*)lv_event_get_user_data(e);
    bool cancelled = (lv_event_get_target(e) == lv_obj_get_child(ctx->container, 0));
    
    if (ctx->callback) {
        const char *text = cancelled ? NULL : lv_textarea_get_text(ctx->textarea);
        ctx->callback(text, cancelled);
    }
    
    lv_obj_del_async(ctx->container);
    free(ctx);
}

lv_obj_t* ui_keyboard_show(lv_obj_t *parent, const char *title, const char *placeholder, ui_keyboard_callback_t callback) {
    if (!parent) return NULL;
    
    ui_keyboard_ctx_t *ctx = (ui_keyboard_ctx_t*)malloc(sizeof(ui_keyboard_ctx_t));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(ui_keyboard_ctx_t));
    ctx->callback = callback;
    
    ctx->container = lv_obj_create(parent);
    lv_obj_set_size(ctx->container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(ctx->container, UI_COLOR_BG_PRIMARY, 0);
    lv_obj_set_style_border_width(ctx->container, 0, 0);
    lv_obj_set_style_pad_all(ctx->container, UI_PADDING_MEDIUM, 0);
    lv_obj_clear_flag(ctx->container, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *title_label = lv_label_create(ctx->container);
    lv_label_set_text(title_label, title ? title : "Enter Text");
    lv_obj_set_style_text_font(title_label, UI_FONT_MEDIUM, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);
    
    ctx->textarea = lv_textarea_create(ctx->container);
    lv_obj_set_size(ctx->textarea, LV_PCT(100), 40);
    lv_obj_align(ctx->textarea, LV_ALIGN_TOP_MID, 0, 25);
    lv_textarea_set_placeholder_text(ctx->textarea, placeholder ? placeholder : "");
    lv_textarea_set_one_line(ctx->textarea, true);
    lv_textarea_set_password_mode(ctx->textarea, false);
    
    ctx->keyboard = lv_keyboard_create(ctx->container);
    lv_obj_set_size(ctx->keyboard, LV_PCT(100), LV_PCT(50));
    lv_obj_align(ctx->keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(ctx->keyboard, ctx->textarea);
    
    lv_obj_t *btn_container = lv_obj_create(ctx->container);
    lv_obj_set_size(btn_container, LV_PCT(100), 50);
    lv_obj_align(btn_container, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    ui_button_config_t cancel_cfg = {
        .label = "Cancel", .icon = NULL, .callback = NULL,
        .bg_color = UI_COLOR_ERROR, .text_color = UI_COLOR_TEXT_PRIMARY, .full_width = false
    };
    lv_obj_t *cancel_btn = ui_button_create(btn_container, &cancel_cfg);
    lv_obj_add_event_cb(cancel_btn, keyboard_btn_event_cb, LV_EVENT_CLICKED, ctx);
    
    ui_button_config_t ok_cfg = {
        .label = "OK", .icon = LV_SYMBOL_OK, .callback = NULL,
        .bg_color = UI_COLOR_SUCCESS, .text_color = UI_COLOR_TEXT_PRIMARY, .full_width = false
    };
    lv_obj_t *ok_btn = ui_button_create(btn_container, &ok_cfg);
    lv_obj_add_event_cb(ok_btn, keyboard_btn_event_cb, LV_EVENT_CLICKED, ctx);
    
    return ctx->container;
}

void ui_keyboard_hide(lv_obj_t *keyboard) {
    if (keyboard) lv_obj_del_async(keyboard);
}
