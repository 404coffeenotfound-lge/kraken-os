/**
 * @file network_ui.h
 * @brief Network menu UI for WiFi management
 */

#ifndef NETWORK_UI_H
#define NETWORK_UI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create network UI screen
 * @param parent Parent container
 * @return Created UI object
 */
lv_obj_t* network_ui_create(lv_obj_t *parent);

/**
 * @brief Destroy network UI screen
 * @param ui UI object to destroy
 */
void network_ui_destroy(lv_obj_t *ui);

/**
 * @brief Update WiFi list with scan results
 * @param ui UI object
 */
void network_ui_update_wifi_list(lv_obj_t *ui);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_UI_H
