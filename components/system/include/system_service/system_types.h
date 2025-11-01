#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SYSTEM_SERVICE_MAX_NAME_LEN     32
#define SYSTEM_SERVICE_MAX_SERVICES     16
#define SYSTEM_SERVICE_MAX_EVENT_TYPES  64
#define SYSTEM_SERVICE_MAX_SUBSCRIBERS  32

typedef uint32_t system_secure_key_t;
typedef uint16_t system_service_id_t;
typedef uint16_t system_event_type_t;

typedef enum {
    SYSTEM_SERVICE_STATE_UNREGISTERED = 0,
    SYSTEM_SERVICE_STATE_REGISTERED,
    SYSTEM_SERVICE_STATE_RUNNING,
    SYSTEM_SERVICE_STATE_PAUSED,
    SYSTEM_SERVICE_STATE_ERROR,
    SYSTEM_SERVICE_STATE_STOPPING,
} system_service_state_t;

typedef enum {
    SYSTEM_EVENT_PRIORITY_LOW = 0,
    SYSTEM_EVENT_PRIORITY_NORMAL,
    SYSTEM_EVENT_PRIORITY_HIGH,
    SYSTEM_EVENT_PRIORITY_CRITICAL,
} system_event_priority_t;

typedef struct {
    system_event_type_t event_type;
    system_event_priority_t priority;
    void *data;
    size_t data_size;
    uint32_t timestamp;
    system_service_id_t sender_id;
} system_event_t;

typedef void (*system_event_handler_t)(const system_event_t *event, void *user_data);

typedef struct {
    char name[SYSTEM_SERVICE_MAX_NAME_LEN];
    system_service_id_t service_id;
    system_service_state_t state;
    uint32_t last_heartbeat;
    void *service_context;
} system_service_info_t;

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_TYPES_H
