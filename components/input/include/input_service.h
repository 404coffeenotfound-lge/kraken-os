#ifndef INPUT_SERVICE_H
#define INPUT_SERVICE_H

#include "esp_err.h"
#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INPUT_EVENT_KEY_LEFT_PRESSED = 0,
    INPUT_EVENT_KEY_RIGHT_PRESSED,
    INPUT_EVENT_KEY_UP_PRESSED,
    INPUT_EVENT_KEY_DOWN_PRESSED,
    INPUT_EVENT_KEY_SELECT_PRESSED,
} input_event_id_t;

esp_err_t input_service_init(void);

esp_err_t input_service_deinit(void);

esp_err_t input_service_start(void);

esp_err_t input_service_stop(void);

system_service_id_t input_service_get_id(void);

#ifdef __cplusplus
}
#endif

#endif // INPUT_SERVICE_H