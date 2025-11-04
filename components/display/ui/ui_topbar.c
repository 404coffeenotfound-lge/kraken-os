/**
 * @file ui_topbar.c
 * @brief iPhone-style fixed topbar implementation
 */

#include "ui_topbar.h"
#include <stdio.h>
#include <string.h>

// Topbar elements
static lv_obj_t *topbar_container = NULL;
static lv_obj_t *clock_label = NULL;
static lv_obj_t *wifi_icon = NULL;
static lv_obj_t *bluetooth_icon = NULL;
static lv_obj_t *battery_icon = NULL;
static lv_obj_t *separator = NULL;

// Configuration
static ui_topbar_config_t current_config;

// Default configuration
static ui_topbar_config_t default_config = {
    .height = 40,
    .separator_height = 2,
};

lv_obj_t* ui_topbar_create(lv_obj_t *parent, const ui_topbar_config_t *config) {
    if (topbar_container != NULL) {
        return topbar_container; // Already created
    }

    // Use default config if none provided
    if (config == NULL) {
        default_config.bg_color = lv_color_hex(0x000000);   // Black
        default_config.text_color = lv_color_hex(0xFFFFFF); // White
        current_config = default_config;
    } else {
        current_config = *config;
    }

    // Create topbar container
    topbar_container = lv_obj_create(parent);
    lv_obj_set_size(topbar_container, LV_HOR_RES, current_config.height);
    lv_obj_set_pos(topbar_container, 0, 0);
    lv_obj_clear_flag(topbar_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(topbar_container, current_config.bg_color, 0);
    lv_obj_set_style_border_width(topbar_container, 0, 0);
    lv_obj_set_style_radius(topbar_container, 0, 0);
    lv_obj_set_style_pad_all(topbar_container, 0, 0);
    lv_obj_set_style_bg_opa(topbar_container, LV_OPA_COVER, 0);

    // Create clock label (top-left)
    clock_label = lv_label_create(topbar_container);
    lv_label_set_text(clock_label, "00:00");
    lv_obj_set_style_text_color(clock_label, current_config.text_color, 0);
    lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_opa(clock_label, LV_OPA_COVER, 0); // Disable anti-aliasing
    lv_obj_align(clock_label, LV_ALIGN_LEFT_MID, 10, 0);

    // Create status icons container (top-right)
    lv_obj_t *icons_container = lv_obj_create(topbar_container);
    lv_obj_set_size(icons_container, 120, current_config.height);
    lv_obj_align(icons_container, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_clear_flag(icons_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(icons_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(icons_container, 0, 0);
    lv_obj_set_style_pad_all(icons_container, 0, 0);
    lv_obj_set_flex_flow(icons_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(icons_container, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(icons_container, 8, 0);

    // WiFi icon
    wifi_icon = lv_label_create(icons_container);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x808080), 0); // Gray (disconnected)
    lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_opa(wifi_icon, LV_OPA_COVER, 0); // Disable anti-aliasing

    // Bluetooth icon
    bluetooth_icon = lv_label_create(icons_container);
    lv_label_set_text(bluetooth_icon, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_color(bluetooth_icon, lv_color_hex(0x808080), 0); // Gray (disconnected)
    lv_obj_set_style_text_font(bluetooth_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_opa(bluetooth_icon, LV_OPA_COVER, 0); // Disable anti-aliasing

    // Battery icon
    battery_icon = lv_label_create(icons_container);
    lv_label_set_text(battery_icon, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_color(battery_icon, current_config.text_color, 0);
    lv_obj_set_style_text_font(battery_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_opa(battery_icon, LV_OPA_COVER, 0); // Disable anti-aliasing

    // Create separator line
    separator = lv_obj_create(parent);
    lv_obj_set_size(separator, LV_HOR_RES, current_config.separator_height);
    lv_obj_set_pos(separator, 0, current_config.height);
    lv_obj_set_style_bg_color(separator, lv_color_hex(0x404040), 0); // Dark gray
    lv_obj_set_style_border_width(separator, 0, 0);
    lv_obj_set_style_radius(separator, 0, 0);
    lv_obj_clear_flag(separator, LV_OBJ_FLAG_SCROLLABLE);

    return topbar_container;
}

void ui_topbar_update_time(uint8_t hour, uint8_t minute) {
    if (clock_label == NULL) return;

    char time_str[8];  // HH:MM\0 = 6 chars minimum
    snprintf(time_str, sizeof(time_str), "%02d:%02d", hour, minute);
    lv_label_set_text(clock_label, time_str);
}

void ui_topbar_update_wifi(bool connected, uint8_t signal_strength) {
    if (wifi_icon == NULL) return;

    if (connected) {
        lv_obj_set_style_text_color(wifi_icon, current_config.text_color, 0);
    } else {
        lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x808080), 0); // Gray
    }
}

void ui_topbar_update_bluetooth(bool connected) {
    if (bluetooth_icon == NULL) return;

    if (connected) {
        lv_obj_set_style_text_color(bluetooth_icon, lv_color_hex(0x0080FF), 0); // Blue
    } else {
        lv_obj_set_style_text_color(bluetooth_icon, lv_color_hex(0x808080), 0); // Gray
    }
}

void ui_topbar_update_battery(uint8_t percentage, bool charging) {
    if (battery_icon == NULL) return;

    const char *symbol;
    lv_color_t color;

    if (charging) {
        symbol = LV_SYMBOL_CHARGE;
        color = lv_color_hex(0x00FF00); // Green
    } else if (percentage > 75) {
        symbol = LV_SYMBOL_BATTERY_FULL;
        color = current_config.text_color;
    } else if (percentage > 50) {
        symbol = LV_SYMBOL_BATTERY_3;
        color = current_config.text_color;
    } else if (percentage > 25) {
        symbol = LV_SYMBOL_BATTERY_2;
        color = lv_color_hex(0xFFFF00); // Yellow
    } else if (percentage > 10) {
        symbol = LV_SYMBOL_BATTERY_1;
        color = lv_color_hex(0xFF8000); // Orange
    } else {
        symbol = LV_SYMBOL_BATTERY_EMPTY;
        color = lv_color_hex(0xFF0000); // Red
    }

    lv_label_set_text(battery_icon, symbol);
    lv_obj_set_style_text_color(battery_icon, color, 0);
}

uint16_t ui_topbar_get_height(void) {
    return current_config.height + current_config.separator_height;
}

void ui_topbar_destroy(void) {
    if (separator != NULL) {
        lv_obj_del(separator);
        separator = NULL;
    }
    
    if (topbar_container != NULL) {
        lv_obj_del(topbar_container);
        topbar_container = NULL;
    }

    clock_label = NULL;
    wifi_icon = NULL;
    bluetooth_icon = NULL;
    battery_icon = NULL;
}
