/**
 * @file app_lifecycle.c
 * @brief Application lifecycle management implementation
 */

#include "app_lifecycle.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "system_service/event_bus.h"
#include <string.h>

static const char *TAG = "app_lifecycle";

/* ============================================================================
 * Subscription Tracking Structure
 * ============================================================================ */

#define MAX_TRACKED_SUBSCRIPTIONS 32

typedef struct {
    system_service_id_t service_id;
    system_event_type_t event_types[MAX_TRACKED_SUBSCRIPTIONS];
    uint8_t subscription_count;
} app_subscription_tracker_t;

/* ============================================================================
 * Global State
 * ============================================================================ */

static app_subscription_tracker_t g_trackers[SYSTEM_SERVICE_MAX_SERVICES] = {0};
static SemaphoreHandle_t g_tracker_mutex = NULL;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static void ensure_mutex(void)
{
    if (g_tracker_mutex == NULL) {
        g_tracker_mutex = xSemaphoreCreateMutex();
    }
}

static app_subscription_tracker_t* find_tracker(system_service_id_t service_id)
{
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (g_trackers[i].service_id == service_id && g_trackers[i].subscription_count > 0) {
            return &g_trackers[i];
        }
    }
    return NULL;
}

static app_subscription_tracker_t* get_or_create_tracker(system_service_id_t service_id)
{
    // Try to find existing
    app_subscription_tracker_t *tracker = find_tracker(service_id);
    if (tracker != NULL) {
        return tracker;
    }
    
    // Find empty slot
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (g_trackers[i].subscription_count == 0) {
            g_trackers[i].service_id = service_id;
            return &g_trackers[i];
        }
    }
    
    return NULL;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

esp_err_t app_lifecycle_track_subscription(system_service_id_t service_id,
                                            system_event_type_t event_type)
{
    ensure_mutex();
    
    if (xSemaphoreTake(g_tracker_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    app_subscription_tracker_t *tracker = get_or_create_tracker(service_id);
    if (tracker == NULL) {
        xSemaphoreGive(g_tracker_mutex);
        ESP_LOGE(TAG, "No tracker slots available");
        return ESP_ERR_NO_MEM;
    }
    
    // Check if already tracked
    for (int i = 0; i < tracker->subscription_count; i++) {
        if (tracker->event_types[i] == event_type) {
            xSemaphoreGive(g_tracker_mutex);
            return ESP_OK; // Already tracked
        }
    }
    
    // Add to tracker
    if (tracker->subscription_count >= MAX_TRACKED_SUBSCRIPTIONS) {
        xSemaphoreGive(g_tracker_mutex);
        ESP_LOGW(TAG, "Max subscriptions tracked for service %d", service_id);
        return ESP_ERR_NO_MEM;
    }
    
    tracker->event_types[tracker->subscription_count++] = event_type;
    
    xSemaphoreGive(g_tracker_mutex);
    
    ESP_LOGD(TAG, "Tracked subscription: service_id=%d, event_type=%d (total=%d)",
             service_id, event_type, tracker->subscription_count);
    
    return ESP_OK;
}

esp_err_t app_lifecycle_untrack_subscription(system_service_id_t service_id,
                                              system_event_type_t event_type)
{
    ensure_mutex();
    
    if (xSemaphoreTake(g_tracker_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    app_subscription_tracker_t *tracker = find_tracker(service_id);
    if (tracker == NULL) {
        xSemaphoreGive(g_tracker_mutex);
        return ESP_OK; // Not tracked
    }
    
    // Find and remove
    for (int i = 0; i < tracker->subscription_count; i++) {
        if (tracker->event_types[i] == event_type) {
            // Shift remaining entries
            for (int j = i; j < tracker->subscription_count - 1; j++) {
                tracker->event_types[j] = tracker->event_types[j + 1];
            }
            tracker->subscription_count--;
            break;
        }
    }
    
    xSemaphoreGive(g_tracker_mutex);
    
    return ESP_OK;
}

esp_err_t app_lifecycle_unsubscribe_all(system_service_id_t service_id)
{
    ensure_mutex();
    
    if (xSemaphoreTake(g_tracker_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    app_subscription_tracker_t *tracker = find_tracker(service_id);
    if (tracker == NULL) {
        xSemaphoreGive(g_tracker_mutex);
        return ESP_OK; // No subscriptions
    }
    
    ESP_LOGI(TAG, "Unsubscribing all events for service %d (%d subscriptions)",
             service_id, tracker->subscription_count);
    
    // Unsubscribe from all tracked events
    for (int i = 0; i < tracker->subscription_count; i++) {
        esp_err_t ret = system_event_unsubscribe(service_id, tracker->event_types[i]);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to unsubscribe from event %d: %s",
                     tracker->event_types[i], esp_err_to_name(ret));
        }
    }
    
    // Clear tracker
    tracker->subscription_count = 0;
    
    xSemaphoreGive(g_tracker_mutex);
    
    ESP_LOGI(TAG, "All subscriptions cleared for service %d", service_id);
    
    return ESP_OK;
}

esp_err_t app_lifecycle_get_subscription_count(system_service_id_t service_id,
                                                uint32_t *count)
{
    if (count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ensure_mutex();
    
    if (xSemaphoreTake(g_tracker_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    app_subscription_tracker_t *tracker = find_tracker(service_id);
    *count = (tracker != NULL) ? tracker->subscription_count : 0;
    
    xSemaphoreGive(g_tracker_mutex);
    
    return ESP_OK;
}
