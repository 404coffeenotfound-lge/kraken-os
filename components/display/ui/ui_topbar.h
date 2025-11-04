/**
 * @file ui_topbar.h
 * @brief iPhone-style fixed topbar with clock and status icons
 */

#pragma once

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Topbar configuration
 */
typedef struct {
    uint16_t height;           // Topbar height in pixels (default: 40)
    lv_color_t bg_color;       // Background color (default: black)
    lv_color_t text_color;     // Text/icon color (default: white)
    uint8_t separator_height;  // Separator line height (default: 2)
} ui_topbar_config_t;

/**
 * @brief Initialize and create the fixed topbar
 * 
 * @param parent Parent object (usually lv_scr_act())
 * @param config Topbar configuration (NULL for defaults)
 * @return lv_obj_t* Topbar container object
 */
lv_obj_t* ui_topbar_create(lv_obj_t *parent, const ui_topbar_config_t *config);

/**
 * @brief Update the clock time display
 * 
 * @param hour Hour (0-23)
 * @param minute Minute (0-59)
 */
void ui_topbar_update_time(uint8_t hour, uint8_t minute);

/**
 * @brief Update WiFi status icon
 * 
 * @param connected WiFi connection status
 * @param signal_strength Signal strength (0-100)
 */
void ui_topbar_update_wifi(bool connected, uint8_t signal_strength);

/**
 * @brief Update Bluetooth status icon
 * 
 * @param connected Bluetooth connection status
 */
void ui_topbar_update_bluetooth(bool connected);

/**
 * @brief Update battery status icon
 * 
 * @param percentage Battery percentage (0-100)
 * @param charging Charging status
 */
void ui_topbar_update_battery(uint8_t percentage, bool charging);

/**
 * @brief Get the topbar height (for positioning other elements)
 * 
 * @return uint16_t Total height including separator
 */
uint16_t ui_topbar_get_height(void);

/**
 * @brief Destroy the topbar
 */
void ui_topbar_destroy(void);

#ifdef __cplusplus
}
#endif
