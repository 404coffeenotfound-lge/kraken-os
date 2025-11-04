#include "system_service/common_events.h"
#include "system_service/event_bus.h"
#include "esp_log.h"

static const char *TAG = "common_events";

typedef struct {
    system_event_type_t id;
    const char *name;
} common_event_entry_t;

static const common_event_entry_t common_events[] = {
    // System events
    {COMMON_EVENT_SYSTEM_STARTUP,    "system.startup"},
    {COMMON_EVENT_SYSTEM_SHUTDOWN,   "system.shutdown"},
    {COMMON_EVENT_SYSTEM_ERROR,      "system.error"},
    
    // Network events
    {COMMON_EVENT_NETWORK_CONNECTED,    "network.connected"},
    {COMMON_EVENT_NETWORK_DISCONNECTED, "network.disconnected"},
    {COMMON_EVENT_NETWORK_GOT_IP,       "network.got_ip"},
    {COMMON_EVENT_NETWORK_LOST_IP,      "network.lost_ip"},
    
    // App events
    {COMMON_EVENT_APP_STARTED,  "app.started"},
    {COMMON_EVENT_APP_STOPPED,  "app.stopped"},
    {COMMON_EVENT_APP_ERROR,    "app.error"},
    
    // User events
    {COMMON_EVENT_USER_INPUT,   "user.input"},
    {COMMON_EVENT_USER_BUTTON,  "user.button"},
};

static const size_t num_common_events = sizeof(common_events) / sizeof(common_events[0]);

esp_err_t common_events_init(void) {
    ESP_LOGI(TAG, "Registering %d common event types...", num_common_events);
    
    for (size_t i = 0; i < num_common_events; i++) {
        system_event_type_t registered_id;
        esp_err_t ret = system_event_register_type(common_events[i].name, &registered_id);
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register '%s': %d", common_events[i].name, ret);
            continue;
        }
        
        // Verify the ID matches expected
        if (registered_id != common_events[i].id) {
            ESP_LOGW(TAG, "Event '%s' got ID %d, expected %d",
                     common_events[i].name, registered_id, common_events[i].id);
        }
    }
    
    ESP_LOGI(TAG, "✓ Common events registered");
    return ESP_OK;
}

const char* common_event_get_name(system_event_type_t event_id) {
    for (size_t i = 0; i < num_common_events; i++) {
        if (common_events[i].id == event_id) {
            return common_events[i].name;
        }
    }
    return NULL;
}
