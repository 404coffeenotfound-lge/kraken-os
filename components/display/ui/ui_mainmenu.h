/**
 * @file ui_mainmenu.h
 * @brief iPhone-style main menu with icons
 */

#pragma once

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Menu item callback function
 */
typedef void (*ui_menu_item_callback_t)(void);

/**
 * @brief Menu item configuration
 */
typedef struct {
    const char *label;              // Menu item text
    const char *icon;               // LVGL symbol (e.g., LV_SYMBOL_AUDIO)
    ui_menu_item_callback_t callback; // Callback when item is clicked
} ui_menu_item_t;

/**
 * @brief Main menu configuration
 */
typedef struct {
    lv_color_t bg_color;       // Background color (default: black)
    lv_color_t text_color;     // Text color (default: white)
    lv_color_t selected_color; // Selected item color (default: dark gray)
    uint16_t item_height;      // Menu item height (default: 60)
} ui_mainmenu_config_t;

/**
 * @brief Create the main menu below the topbar
 * 
 * @param parent Parent object (usually lv_scr_act())
 * @param topbar_height Height of the topbar to offset menu
 * @param items Array of menu items
 * @param item_count Number of menu items
 * @param config Menu configuration (NULL for defaults)
 * @return lv_obj_t* Main menu container object
 */
lv_obj_t* ui_mainmenu_create(lv_obj_t *parent, uint16_t topbar_height,
                             const ui_menu_item_t *items, size_t item_count,
                             const ui_mainmenu_config_t *config);

/**
 * @brief Destroy the main menu
 */
void ui_mainmenu_destroy(void);

/**
 * @brief Show the main menu
 */
void ui_mainmenu_show(void);

/**
 * @brief Hide the main menu (when switching to another window)
 */
void ui_mainmenu_hide(void);

#ifdef __cplusplus
}
#endif
