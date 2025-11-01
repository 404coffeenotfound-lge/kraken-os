#ifndef SYSTEM_INTERNAL_H
#define SYSTEM_INTERNAL_H

#include "system_service/system_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SYSTEM_MAGIC_NUMBER           0x53595354
#define SYSTEM_EVENT_QUEUE_SIZE       32
#define SYSTEM_MAX_DATA_SIZE          512

typedef struct {
    system_event_type_t event_type;
    char event_name[SYSTEM_SERVICE_MAX_NAME_LEN];
    bool registered;
} event_type_entry_t;

typedef struct {
    system_service_id_t service_id;
    system_event_type_t event_type;
    system_event_handler_t handler;
    void *user_data;
    bool active;
} event_subscription_t;

typedef struct {
    char name[SYSTEM_SERVICE_MAX_NAME_LEN];
    system_service_id_t service_id;
    system_service_state_t state;
    uint32_t last_heartbeat;
    void *service_context;
    bool registered;
    uint32_t event_count;
} service_entry_t;

typedef struct {
    uint32_t magic;
    bool initialized;
    system_secure_key_t secure_key;
    
    service_entry_t services[SYSTEM_SERVICE_MAX_SERVICES];
    uint16_t service_count;
    
    event_type_entry_t event_types[SYSTEM_SERVICE_MAX_EVENT_TYPES];
    uint16_t event_type_count;
    
    event_subscription_t subscriptions[SYSTEM_SERVICE_MAX_SUBSCRIBERS];
    uint16_t subscription_count;
    
    QueueHandle_t event_queue;
    SemaphoreHandle_t mutex;
    TaskHandle_t event_task;
    
    uint32_t total_events_posted;
    uint32_t total_events_processed;
    
    bool running;
} system_context_t;

bool system_verify_key(system_secure_key_t key);

system_context_t* system_get_context(void);

esp_err_t system_lock(void);

esp_err_t system_unlock(void);

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_INTERNAL_H
