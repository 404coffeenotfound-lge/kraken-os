/**
 * @file ui_styles.h
 * @brief Common UI styles and theme colors for Kraken OS
 */

#ifndef UI_STYLES_H
#define UI_STYLES_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Color palette
#define UI_COLOR_BG_PRIMARY         lv_color_hex(0x000000)   // Black
#define UI_COLOR_BG_SECONDARY       lv_color_hex(0x1A1A1A)   // Dark gray
#define UI_COLOR_BG_SELECTED        lv_color_hex(0x333333)   // Selected gray
#define UI_COLOR_TEXT_PRIMARY       lv_color_hex(0xFFFFFF)   // White
#define UI_COLOR_TEXT_SECONDARY     lv_color_hex(0x808080)   // Gray
#define UI_COLOR_ACCENT             lv_color_hex(0x0080FF)   // Blue
#define UI_COLOR_SUCCESS            lv_color_hex(0x00FF00)   // Green
#define UI_COLOR_WARNING            lv_color_hex(0xFFFF00)   // Yellow
#define UI_COLOR_ERROR              lv_color_hex(0xFF0000)   // Red
#define UI_COLOR_BORDER             lv_color_hex(0x404040)   // Border gray

// Spacing
#define UI_PADDING_SMALL            4
#define UI_PADDING_MEDIUM           8
#define UI_PADDING_LARGE            12
#define UI_PADDING_XLARGE           16

// Border radius
#define UI_RADIUS_SMALL             4
#define UI_RADIUS_MEDIUM            8
#define UI_RADIUS_LARGE             12

// Font sizes
#define UI_FONT_SMALL               &lv_font_montserrat_12
#define UI_FONT_MEDIUM              &lv_font_montserrat_14
#define UI_FONT_LARGE               &lv_font_montserrat_16

// Common dimensions
#define UI_BUTTON_HEIGHT            40
#define UI_TOGGLE_WIDTH             50
#define UI_TOGGLE_HEIGHT            28
#define UI_LIST_ITEM_HEIGHT         45

#ifdef __cplusplus
}
#endif

#endif // UI_STYLES_H
