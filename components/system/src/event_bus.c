/**
 * @file event_bus.c
 * @brief Event bus implementation with production features
 * 
 * Integrates: memory pools, priority queues, handler monitoring, resource quotas
 */

#include "system_service/event_bus.h"
#include "system_internal.h"
#include "memory_pool.h"
#include "priority_queue.h"
#include "handler_monitor.h"
#include "resource_quota.h"
#include "app_lifecycle.h"
#include "system_service/error_codes.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "event_bus";

esp_err_t system_event_register_type(const char *event_name,
                                      system_event_type_t *out_event_type)
{
    if (event_name == NULL || out_event_type == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_context_t *ctx = system_get_context();
    if (!ctx->initialized) {
        ESP_LOGE(TAG, "System service not initialized");
        return ESP_ERR_SYSTEM_NOT_INITIALIZED;
    }
    
    esp_err_t ret = system_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Check if already registered
    for (int i = 0; i < SYSTEM_SERVICE_MAX_EVENT_TYPES; i++) {
        if (ctx->event_types[i].registered &&
            strcmp(ctx->event_types[i].event_name, event_name) == 0) {
            *out_event_type = ctx->event_types[i].event_type;
            system_unlock();
            ESP_LOGD(TAG, "Event type '%s' already registered", event_name);
            return ESP_OK;
        }
    }
    
    if (ctx->event_type_count >= SYSTEM_SERVICE_MAX_EVENT_TYPES) {
        ESP_LOGE(TAG, "Maximum event types reached");
        system_unlock();
        return ESP_ERR_EVENT_TYPE_REGISTRY_FULL;
    }
    
    // Find free slot
    int slot = -1;
    for (int i = 0; i < SYSTEM_SERVICE_MAX_EVENT_TYPES; i++) {
        if (!ctx->event_types[i].registered) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        system_unlock();
        return ESP_ERR_EVENT_TYPE_REGISTRY_FULL;
    }
    
    strncpy(ctx->event_types[slot].event_name, event_name, SYSTEM_SERVICE_MAX_NAME_LEN - 1);
    ctx->event_types[slot].event_name[SYSTEM_SERVICE_MAX_NAME_LEN - 1] = '\0';
    ctx->event_types[slot].event_type = (system_event_type_t)slot;
    ctx->event_types[slot].registered = true;
    
    *out_event_type = ctx->event_types[slot].event_type;
    ctx->event_type_count++;
    
    system_unlock();
    
    ESP_LOGI(TAG, "Event type '%s' registered with ID %d", event_name, *out_event_type);
    
    return ESP_OK;
}

esp_err_t system_event_subscribe(system_service_id_t service_id,
                                  system_event_type_t event_type,
                                  system_event_handler_t handler,
                                  void *user_data)
{
    if (handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_context_t *ctx = system_get_context();
    if (!ctx->initialized) {
        return ESP_ERR_SYSTEM_NOT_INITIALIZED;
    }
    
    if (service_id >= SYSTEM_SERVICE_MAX_SERVICES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check subscription quota
    esp_err_t ret = quota_check_subscription(service_id);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = system_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (!ctx->services[service_id].registered) {
        system_unlock();
        return ESP_ERR_SERVICE_NOT_FOUND;
    }
    
    if (event_type >= SYSTEM_SERVICE_MAX_EVENT_TYPES ||
        !ctx->event_types[event_type].registered) {
        ESP_LOGE(TAG, "Event type %d not registered", event_type);
        system_unlock();
        return ESP_ERR_EVENT_TYPE_NOT_FOUND;
    }
    
    // Check if already subscribed
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SUBSCRIBERS; i++) {
        if (ctx->subscriptions[i].active &&
            ctx->subscriptions[i].service_id == service_id &&
            ctx->subscriptions[i].event_type == event_type) {
            ESP_LOGD(TAG, "Service %d already subscribed to event %d", service_id, event_type);
            system_unlock();
            return ESP_OK;
        }
    }
    
    // Find free slot
    int slot = -1;
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SUBSCRIBERS; i++) {
        if (!ctx->subscriptions[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        ESP_LOGE(TAG, "Maximum subscriptions reached");
        system_unlock();
        return ESP_ERR_EVENT_SUBSCRIPTION_FULL;
    }
    
    ctx->subscriptions[slot].service_id = service_id;
    ctx->subscriptions[slot].event_type = event_type;
    ctx->subscriptions[slot].handler = handler;
    ctx->subscriptions[slot].user_data = user_data;
    ctx->subscriptions[slot].active = true;
    ctx->subscription_count++;
    
    system_unlock();
    
    // Track subscription for auto-cleanup
    app_lifecycle_track_subscription(service_id, event_type);
    
    // Record in quota system
    quota_record_subscription(service_id, true);
    
    ESP_LOGI(TAG, "Service %d subscribed to event type %d", service_id, event_type);
    
    return ESP_OK;
}

esp_err_t system_event_unsubscribe(system_service_id_t service_id,
                                    system_event_type_t event_type)
{
    system_context_t *ctx = system_get_context();
    if (!ctx->initialized) {
        return ESP_ERR_SYSTEM_NOT_INITIALIZED;
    }
    
    if (service_id >= SYSTEM_SERVICE_MAX_SERVICES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = system_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    bool found = false;
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SUBSCRIBERS; i++) {
        if (ctx->subscriptions[i].active &&
            ctx->subscriptions[i].service_id == service_id &&
            ctx->subscriptions[i].event_type == event_type) {
            ctx->subscriptions[i].active = false;
            ctx->subscription_count--;
            found = true;
            break;
        }
    }
    
    system_unlock();
    
    if (!found) {
        return ESP_ERR_EVENT_SUBSCRIPTION_NOT_FOUND;
    }
    
    // Untrack subscription
    app_lifecycle_untrack_subscription(service_id, event_type);
    
    // Update quota
    quota_record_subscription(service_id, false);
    
    ESP_LOGI(TAG, "Service %d unsubscribed from event type %d", service_id, event_type);
    
    return ESP_OK;
}

esp_err_t system_event_post(system_service_id_t sender_id,
                            system_event_type_t event_type,
                            const void *data,
                            size_t data_size,
                            system_event_priority_t priority)
{
    system_context_t *ctx = system_get_context();
    if (!ctx->initialized || !ctx->running) {
        return ESP_ERR_SYSTEM_NOT_STARTED;
    }
    
    if (sender_id >= SYSTEM_SERVICE_MAX_SERVICES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check event posting quota
    esp_err_t ret = quota_check_event_post(sender_id);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Check data size quota
    ret = quota_check_data_size(sender_id, data_size);
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (data_size > SYSTEM_MAX_DATA_SIZE) {
        ESP_LOGE(TAG, "Data size %zu exceeds maximum %d", data_size, SYSTEM_MAX_DATA_SIZE);
        return ESP_ERR_EVENT_DATA_TOO_LARGE;
    }
    
    ret = system_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (!ctx->services[sender_id].registered) {
        system_unlock();
        return ESP_ERR_SERVICE_NOT_FOUND;
    }
    
    if (event_type >= SYSTEM_SERVICE_MAX_EVENT_TYPES ||
        !ctx->event_types[event_type].registered) {
        system_unlock();
        return ESP_ERR_EVENT_TYPE_NOT_FOUND;
    }
    
    system_event_t event = {0};
    event.event_type = event_type;
    event.priority = priority;
    event.sender_id = sender_id;
    event.timestamp = (uint32_t)(esp_timer_get_time() / 1000);
    event.data_size = data_size;
    
    // Use memory pool for event data allocation
    if (data != NULL && data_size > 0) {
        event.data = memory_pool_alloc(data_size);
        if (event.data == NULL) {
            system_unlock();
            ESP_LOGE(TAG, "Failed to allocate event data");
            return ESP_ERR_NO_MEM;
        }
        memcpy(event.data, data, data_size);
    }
    
    ctx->services[sender_id].event_count++;
    ctx->total_events_posted++;
    
    system_unlock();
    
    // Post to priority queue instead of simple queue
    ret = priority_queue_post(ctx->event_queue, &event, 100);
    if (ret != ESP_OK) {
        if (event.data != NULL) {
            memory_pool_free(event.data);
        }
        ESP_LOGE(TAG, "Failed to post event to priority queue: %s", 
                 system_service_err_to_name(ret));
        return ret;
    }
    
    // Record event posting in quota
    quota_record_event_post(sender_id);
    
    return ESP_OK;
}

esp_err_t system_event_post_async(system_service_id_t sender_id,
                                   system_event_type_t event_type,
                                   const void *data,
                                   size_t data_size,
                                   system_event_priority_t priority)
{
    return system_event_post(sender_id, event_type, data, data_size, priority);
}

esp_err_t system_event_get_type_name(system_event_type_t event_type,
                                      char *out_name,
                                      size_t max_len)
{
    if (out_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    system_context_t *ctx = system_get_context();
    if (!ctx->initialized) {
        return ESP_ERR_SYSTEM_NOT_INITIALIZED;
    }
    
    esp_err_t ret = system_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (event_type >= SYSTEM_SERVICE_MAX_EVENT_TYPES ||
        !ctx->event_types[event_type].registered) {
        system_unlock();
        return ESP_ERR_EVENT_TYPE_NOT_FOUND;
    }
    
    strncpy(out_name, ctx->event_types[event_type].event_name, max_len - 1);
    out_name[max_len - 1] = '\0';
    
    system_unlock();
    
    return ESP_OK;
}
