#include "audio_service.h"
#include "system_service/system_service.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "service_watchdog.h"
#include "resource_quota.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "audio_service";

static system_service_id_t audio_service_id = 0;
static system_event_type_t audio_events[8];
static bool initialized = false;
static uint8_t current_volume = 50;
static bool is_muted = false;

esp_err_t audio_service_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Audio service already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing audio service...");
    
    // Register with system service
    ret = system_service_register("audio_service", NULL, &audio_service_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register with system service: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ Registered with system service (ID: %d)", audio_service_id);
    
    // Register event types
    const char *event_names[] = {
        "audio.registered",
        "audio.started",
        "audio.stopped",
        "audio.volume_changed",
        "audio.playback_state",
        "audio.error"
    };
    
    for (int i = 0; i < 6; i++) {
        ret = system_event_register_type(event_names[i], &audio_events[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register event type '%s'", event_names[i]);
            return ret;
        }
    }
    
    ESP_LOGI(TAG, "✓ Registered %d event types", 6);
    
    // Register with watchdog
    service_watchdog_config_t watchdog_config = {
        .timeout_ms = 30000,              // 30 second timeout
        .auto_restart = true,             // Auto-restart on failure
        .max_restart_attempts = 3,        // Max 3 restart attempts
        .is_critical = false              // Not a critical service
    };
    watchdog_register_service(audio_service_id, &watchdog_config);
    ESP_LOGI(TAG, "✓ Registered with watchdog (30s timeout)");
    
    // Set resource quotas
    service_quota_t quota = {
        .max_events_per_sec = 50,         // Max 50 events/sec
        .max_subscriptions = 8,           // Max 8 subscriptions
        .max_event_data_size = 256,       // Max 256 bytes per event
        .max_memory_bytes = 32 * 1024     // Max 32KB memory
    };
    quota_set(audio_service_id, &quota);
    ESP_LOGI(TAG, "✓ Resource quotas set (50 events/s, 32KB memory)");
    
    // Set service state
    system_service_set_state(audio_service_id, SYSTEM_SERVICE_STATE_REGISTERED);
    
    initialized = true;
    
    // Post registration event
    system_event_post(audio_service_id, 
                     audio_events[AUDIO_EVENT_REGISTERED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Audio service initialized successfully");
    ESP_LOGI(TAG, "  → Posted AUDIO_EVENT_REGISTERED");
    
    return ESP_OK;
}

esp_err_t audio_service_deinit(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing audio service...");
    
    system_service_unregister(audio_service_id);
    initialized = false;
    
    ESP_LOGI(TAG, "✓ Audio service deinitialized");
    
    return ESP_OK;
}

esp_err_t audio_service_start(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting audio service...");
    
    system_service_set_state(audio_service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    // Post started event
    system_event_post(audio_service_id,
                     audio_events[AUDIO_EVENT_STARTED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    // Send heartbeat to watchdog
    system_service_heartbeat(audio_service_id);
    
    ESP_LOGI(TAG, "✓ Audio service started");
    ESP_LOGI(TAG, "  → Posted AUDIO_EVENT_STARTED");
    
    return ESP_OK;
}

esp_err_t audio_service_stop(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Stopping audio service...");
    
    system_service_set_state(audio_service_id, SYSTEM_SERVICE_STATE_STOPPING);
    
    // Post stopped event
    system_event_post(audio_service_id,
                     audio_events[AUDIO_EVENT_STOPPED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Audio service stopped");
    
    return ESP_OK;
}

esp_err_t audio_set_volume(uint8_t volume)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (volume > 100) {
        volume = 100;
    }
    
    current_volume = volume;
    
    audio_volume_event_t event_data = {
        .volume = volume,
        .muted = is_muted
    };
    
    system_event_post(audio_service_id,
                     audio_events[AUDIO_EVENT_VOLUME_CHANGED],
                     &event_data,
                     sizeof(event_data),
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    system_service_heartbeat(audio_service_id);
    
    ESP_LOGI(TAG, "Volume changed: %d%%", volume);
    
    return ESP_OK;
}

esp_err_t audio_get_volume(uint8_t *volume)
{
    if (!initialized || volume == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    *volume = current_volume;
    return ESP_OK;
}

system_service_id_t audio_service_get_id(void)
{
    return audio_service_id;
}
