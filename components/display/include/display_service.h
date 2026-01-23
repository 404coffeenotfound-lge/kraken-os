#ifndef DISPLAY_SERVICE_H
#define DISPLAY_SERVICE_H

#include "esp_err.h"
#include "system_service/system_types.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DISPLAY_EVENT_REGISTERED = 0,
    DISPLAY_EVENT_STARTED,
    DISPLAY_EVENT_STOPPED,
    DISPLAY_EVENT_BRIGHTNESS_CHANGED,
    DISPLAY_EVENT_SCREEN_ON,
    DISPLAY_EVENT_SCREEN_OFF,
    DISPLAY_EVENT_ORIENTATION_CHANGED,
    DISPLAY_EVENT_ERROR,
} display_event_id_t;

typedef enum {
    DISPLAY_ORIENTATION_0 = 0,
    DISPLAY_ORIENTATION_90,
    DISPLAY_ORIENTATION_180,
    DISPLAY_ORIENTATION_270,
} display_orientation_t;

typedef struct {
    uint8_t brightness;
} display_brightness_event_t;

typedef struct {
    display_orientation_t orientation;
} display_orientation_event_t;

esp_err_t display_service_init(void);

esp_err_t display_service_deinit(void);

esp_err_t display_service_start(void);

esp_err_t display_service_stop(void);

esp_err_t display_set_brightness(uint8_t brightness);

esp_err_t display_get_brightness(uint8_t *brightness);

esp_err_t display_screen_on(void);

esp_err_t display_screen_off(void);

esp_err_t display_set_orientation(display_orientation_t orientation);

system_service_id_t display_service_get_id(void);

/**
 * @brief Load a new screen content (destroys old content)
 * @param content New content container to display
 * @return ESP_OK on success
 */
esp_err_t display_service_load_screen(lv_obj_t *content);

/**
 * @brief Get the main LVGL screen object for creating content
 * @return LVGL screen object or NULL
 */
lv_obj_t* display_service_get_main_screen(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_SERVICE_H
