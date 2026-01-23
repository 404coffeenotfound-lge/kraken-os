/**
 * @file ui_mainmenu.c
 * @brief iPhone-style main menu implementation
 * 
 * REDESIGNED: No static caching - always creates fresh objects
 * Caller is responsible for object lifecycle management
 */

#include "ui_mainmenu.h"
#include <string.h>

// Default configuration (no static object pointers!)
static ui_mainmenu_config_t default_config = {
    .item_height = 45,
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
    
    if (parent == NULL || items == NULL || item_count == 0) {
        return NULL;
    }
    
    // Use provided config or defaults
    ui_mainmenu_config_t cfg;
    if (config == NULL) {
        cfg = default_config;
        cfg.bg_color = lv_color_hex(0x000000);       // Black
        cfg.text_color = lv_color_hex(0xFFFFFF);     // White
        cfg.selected_color = lv_color_hex(0x333333); // Dark gray
    } else {
        cfg = *config;
    }

    // Always create new menu container - no caching!
    lv_obj_t *container = lv_obj_create(parent);
    if (container == NULL) {
        return NULL;
    }
    
    lv_obj_set_size(container, LV_HOR_RES, LV_VER_RES - topbar_height);
    lv_obj_set_pos(container, 0, topbar_height);
    lv_obj_set_style_bg_color(container, cfg.bg_color, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_radius(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_AUTO);

    // Create menu items
    for (size_t i = 0; i < item_count; i++) {
        // Create item container
        lv_obj_t *item = lv_obj_create(container);
        lv_obj_set_size(item, LV_HOR_RES, cfg.item_height);
        lv_obj_set_pos(item, 0, i * cfg.item_height);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(item, cfg.bg_color, 0);
        lv_obj_set_style_bg_color(item, cfg.selected_color, LV_STATE_PRESSED);
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
            lv_obj_set_style_text_color(icon, cfg.text_color, 0);
            lv_obj_set_style_text_font(icon, &lv_font_montserrat_14, 0);
            lv_obj_align(icon, LV_ALIGN_LEFT_MID, 12, 0);
        }

        // Create label
        lv_obj_t *label = lv_label_create(item);
        lv_label_set_text(label, items[i].label);
        lv_obj_set_style_text_color(label, cfg.text_color, 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 45, 0);

        // Add chevron (right arrow)
        lv_obj_t *chevron = lv_label_create(item);
        lv_label_set_text(chevron, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(chevron, lv_color_hex(0x808080), 0);
        lv_obj_set_style_text_font(chevron, &lv_font_montserrat_12, 0);
        lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, -8, 0);

        // Add click event
        if (items[i].callback != NULL) {
            lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(item, menu_item_clicked, LV_EVENT_CLICKED, 
                              (void*)items[i].callback);
        }
    }

    return container;
}

// These functions are now no-ops since we don't cache
// Caller should use lv_obj_del() directly on the returned container
void ui_mainmenu_destroy(void) {
    // No-op - caller manages lifecycle
}

void ui_mainmenu_show(void) {
    // No-op - caller manages visibility
}

void ui_mainmenu_hide(void) {
    // No-op - caller manages visibility
}
