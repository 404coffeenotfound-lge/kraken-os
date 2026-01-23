#ifndef POWER_SERVICE_H
#define POWER_SERVICE_H

#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    POWER_EVENT_REGISTERED = 0,
    POWER_EVENT_STARTED,
    POWER_EVENT_STOPPED,
    POWER_EVENT_ERROR,
    POWER_EVENT_BATTERY_LEVEL,
    POWER_EVENT_BATTERY_STATUS,
} power_event_id_t;

typedef struct {
    uint8_t level;
    bool is_charging;
} power_battery_event_t;

esp_err_t power_service_init(void);

esp_err_t power_service_deinit(void);

esp_err_t power_service_start(void);

esp_err_t power_service_stop(void);

system_service_id_t power_service_get_id(void);

#ifdef __cplusplus
}
#endif

#endif // POWER_SERVICE_H   