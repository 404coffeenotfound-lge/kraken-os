#ifndef COMMON_EVENTS_H
#define COMMON_EVENTS_H

#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Common pre-registered event types
 * 
 * These events are registered at system startup and available by ID.
 * Flash apps can use these without needing to register them (which requires strings).
 */

// System events (0-99)
#define COMMON_EVENT_SYSTEM_STARTUP     0
#define COMMON_EVENT_SYSTEM_SHUTDOWN    1
#define COMMON_EVENT_SYSTEM_ERROR       2

// Network events (100-199)
#define COMMON_EVENT_NETWORK_CONNECTED      100
#define COMMON_EVENT_NETWORK_DISCONNECTED   101
#define COMMON_EVENT_NETWORK_GOT_IP         102
#define COMMON_EVENT_NETWORK_LOST_IP        103

// App events (200-299)
#define COMMON_EVENT_APP_STARTED        200
#define COMMON_EVENT_APP_STOPPED        201
#define COMMON_EVENT_APP_ERROR          202

// User events (300+)
#define COMMON_EVENT_USER_INPUT         300
#define COMMON_EVENT_USER_BUTTON        301

/**
 * @brief Initialize common event types
 * 
 * Registers all common events at system startup.
 * Must be called during system initialization.
 */
esp_err_t common_events_init(void);

/**
 * @brief Get event type name
 * 
 * @param event_id Common event ID
 * @return Event name string, or NULL if unknown
 */
const char* common_event_get_name(system_event_type_t event_id);

#ifdef __cplusplus
}
#endif

#endif // COMMON_EVENTS_H
