#include "display_service.h"
#include "system_service/system_service.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "esp_log.h"

static const char *TAG = "display_service";

static system_service_id_t display_service_id = 0;
static system_event_type_t display_events[8];
static bool initialized = false;
static uint8_t current_brightness = 80;
static bool screen_on_state = true;
static display_orientation_t current_orientation = DISPLAY_ORIENTATION_0;

esp_err_t display_service_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Display service already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing display service...");
    
    // Register with system service
    ret = system_service_register("display_service", NULL, &display_service_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register with system service: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ Registered with system service (ID: %d)", display_service_id);
    
    // Register event types
    const char *event_names[] = {
        "display.registered",
        "display.started",
        "display.stopped",
        "display.brightness_changed",
        "display.screen_on",
        "display.screen_off",
        "display.orientation_changed",
        "display.error"
    };
    
    for (int i = 0; i < 8; i++) {
        ret = system_event_register_type(event_names[i], &display_events[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register event type '%s'", event_names[i]);
            return ret;
        }
    }
    
    ESP_LOGI(TAG, "✓ Registered %d event types", 8);
    
    // Set service state
    system_service_set_state(display_service_id, SYSTEM_SERVICE_STATE_REGISTERED);
    
    initialized = true;
    
    // Post registration event
    system_event_post(display_service_id,
                     display_events[DISPLAY_EVENT_REGISTERED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Display service initialized successfully");
    ESP_LOGI(TAG, "  → Posted DISPLAY_EVENT_REGISTERED");
    
    return ESP_OK;
}

esp_err_t display_service_deinit(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing display service...");
    
    system_service_unregister(display_service_id);
    initialized = false;
    
    ESP_LOGI(TAG, "✓ Display service deinitialized");
    
    return ESP_OK;
}

esp_err_t display_service_start(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting display service...");
    
    system_service_set_state(display_service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    // Post started event
    system_event_post(display_service_id,
                     display_events[DISPLAY_EVENT_STARTED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Display service started");
    ESP_LOGI(TAG, "  → Posted DISPLAY_EVENT_STARTED");
    
    return ESP_OK;
}

esp_err_t display_service_stop(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Stopping display service...");
    
    system_service_set_state(display_service_id, SYSTEM_SERVICE_STATE_STOPPING);
    
    // Post stopped event
    system_event_post(display_service_id,
                     display_events[DISPLAY_EVENT_STOPPED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Display service stopped");
    
    return ESP_OK;
}

esp_err_t display_set_brightness(uint8_t brightness)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (brightness > 100) {
        brightness = 100;
    }
    
    current_brightness = brightness;
    
    display_brightness_event_t event_data = {
        .brightness = brightness
    };
    
    system_event_post(display_service_id,
                     display_events[DISPLAY_EVENT_BRIGHTNESS_CHANGED],
                     &event_data,
                     sizeof(event_data),
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    system_service_heartbeat(display_service_id);
    
    ESP_LOGI(TAG, "Brightness changed: %d%%", brightness);
    
    return ESP_OK;
}

esp_err_t display_get_brightness(uint8_t *brightness)
{
    if (!initialized || brightness == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    *brightness = current_brightness;
    return ESP_OK;
}

esp_err_t display_screen_on(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Turning screen ON");
    
    screen_on_state = true;
    
    system_event_post(display_service_id,
                     display_events[DISPLAY_EVENT_SCREEN_ON],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    system_service_heartbeat(display_service_id);
    
    return ESP_OK;
}

esp_err_t display_screen_off(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Turning screen OFF");
    
    screen_on_state = false;
    
    system_event_post(display_service_id,
                     display_events[DISPLAY_EVENT_SCREEN_OFF],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    system_service_heartbeat(display_service_id);
    
    return ESP_OK;
}

esp_err_t display_set_orientation(display_orientation_t orientation)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    current_orientation = orientation;
    
    display_orientation_event_t event_data = {
        .orientation = orientation
    };
    
    system_event_post(display_service_id,
                     display_events[DISPLAY_EVENT_ORIENTATION_CHANGED],
                     &event_data,
                     sizeof(event_data),
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    system_service_heartbeat(display_service_id);
    
    ESP_LOGI(TAG, "Orientation changed: %d degrees", orientation * 90);
    
    return ESP_OK;
}

system_service_id_t display_service_get_id(void)
{
    return display_service_id;
}
