/**
 * @file ui_mainmenu.c
 * @brief iPhone-style main menu implementation
 */

#include "ui_mainmenu.h"
#include <string.h>

// Menu state
static lv_obj_t *menu_container = NULL;
static ui_mainmenu_config_t current_config;

// Default configuration
static ui_mainmenu_config_t default_config = {
    .item_height = 60,
};

// Menu item click callback
static void menu_item_clicked(lv_event_t *e) {
    ui_menu_item_callback_t callback = (ui_menu_item_callback_t)lv_event_get_user_data(e);
    if (callback != NULL) {
        callback();
    }
}

lv_obj_t* ui_mainmenu_create(lv_obj_t *parent, uint16_t topbar_height,
                             const ui_menu_item_t *items, size_t item_count,
                             const ui_mainmenu_config_t *config) {
    if (menu_container != NULL) {
        return menu_container; // Already created
    }

    // Use default config if none provided
    if (config == NULL) {
        default_config.bg_color = lv_color_hex(0x000000);       // Black
        default_config.text_color = lv_color_hex(0xFFFFFF);     // White
        default_config.selected_color = lv_color_hex(0x333333); // Dark gray
        current_config = default_config;
    } else {
        current_config = *config;
    }

    // Create menu container
    menu_container = lv_obj_create(parent);
    lv_obj_set_size(menu_container, LV_HOR_RES, LV_VER_RES - topbar_height);
    lv_obj_set_pos(menu_container, 0, topbar_height);
    lv_obj_set_style_bg_color(menu_container, current_config.bg_color, 0);
    lv_obj_set_style_border_width(menu_container, 0, 0);
    lv_obj_set_style_radius(menu_container, 0, 0);
    lv_obj_set_style_pad_all(menu_container, 0, 0);
    lv_obj_set_scrollbar_mode(menu_container, LV_SCROLLBAR_MODE_AUTO);

    // Create menu items
    for (size_t i = 0; i < item_count; i++) {
        // Create item container
        lv_obj_t *item = lv_obj_create(menu_container);
        lv_obj_set_size(item, LV_HOR_RES, current_config.item_height);
        lv_obj_set_pos(item, 0, i * current_config.item_height);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(item, current_config.bg_color, 0);
        lv_obj_set_style_bg_color(item, current_config.selected_color, LV_STATE_PRESSED);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 10, 0);

        // Add bottom border (separator)
        if (i < item_count - 1) {
            lv_obj_set_style_border_side(item, LV_BORDER_SIDE_BOTTOM, 0);
            lv_obj_set_style_border_width(item, 1, 0);
            lv_obj_set_style_border_color(item, lv_color_hex(0x404040), 0);
        }

        // Create icon
        if (items[i].icon != NULL) {
            lv_obj_t *icon = lv_label_create(item);
            lv_label_set_text(icon, items[i].icon);
            lv_obj_set_style_text_color(icon, current_config.text_color, 0);
            lv_obj_set_style_text_font(icon, &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_opa(icon, LV_OPA_COVER, 0); // Disable anti-aliasing
            lv_obj_align(icon, LV_ALIGN_LEFT_MID, 15, 0);
        }

        // Create label
        lv_obj_t *label = lv_label_create(item);
        lv_label_set_text(label, items[i].label);
        lv_obj_set_style_text_color(label, current_config.text_color, 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0); // Disable anti-aliasing
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 55, 0);

        // Add chevron (right arrow)
        lv_obj_t *chevron = lv_label_create(item);
        lv_label_set_text(chevron, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(chevron, lv_color_hex(0x808080), 0);
        lv_obj_set_style_text_font(chevron, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_opa(chevron, LV_OPA_COVER, 0); // Disable anti-aliasing
        lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, -10, 0);

        // Add click event
        if (items[i].callback != NULL) {
            lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(item, menu_item_clicked, LV_EVENT_CLICKED, 
                              (void*)items[i].callback);
        }
    }

    return menu_container;
}

void ui_mainmenu_destroy(void) {
    if (menu_container != NULL) {
        lv_obj_del(menu_container);
        menu_container = NULL;
    }
}

void ui_mainmenu_show(void) {
    if (menu_container != NULL) {
        lv_obj_clear_flag(menu_container, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_mainmenu_hide(void) {
    if (menu_container != NULL) {
        lv_obj_add_flag(menu_container, LV_OBJ_FLAG_HIDDEN);
    }
}
