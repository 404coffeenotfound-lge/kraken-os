/**
 * @file handler_monitor.c
 * @brief Event handler execution monitoring implementation
 */

#include "handler_monitor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "system_service/error_codes.h"

static const char *TAG = "handler_monitor";

/* ============================================================================
 * Handler Statistics (per service)
 * ============================================================================ */

typedef struct {
    uint64_t total_time_us;
    uint32_t execution_count;
    uint32_t max_time_us;
    uint32_t timeout_count;
} handler_stats_t;

static handler_stats_t g_handler_stats[SYSTEM_SERVICE_MAX_SERVICES] = {0};

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

esp_err_t handler_monitor_execute(system_event_handler_t handler,
                                   const system_event_t *event,
                                   void *user_data,
                                   system_service_id_t service_id)
{
    if (handler == NULL || event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
#if CONFIG_SYSTEM_SERVICE_ENABLE_HANDLER_MONITORING
    // Record start time
    int64_t start_time = esp_timer_get_time();
    
    // Execute handler
    handler(event, user_data);
    
    // Calculate execution time
    int64_t end_time = esp_timer_get_time();
    uint32_t elapsed_us = (uint32_t)(end_time - start_time);
    
    // Update statistics
    if (service_id < SYSTEM_SERVICE_MAX_SERVICES) {
        handler_stats_t *stats = &g_handler_stats[service_id];
        stats->total_time_us += elapsed_us;
        stats->execution_count++;
        
        if (elapsed_us > stats->max_time_us) {
            stats->max_time_us = elapsed_us;
        }
    }
    
    // Check for slow handler
    uint32_t warn_threshold_us = CONFIG_SYSTEM_SERVICE_HANDLER_WARN_THRESHOLD_MS * 1000;
    if (elapsed_us > warn_threshold_us) {
        ESP_LOGW(TAG, "Slow handler detected: service_id=%d, time=%lu us (threshold=%lu us)",
                 service_id, elapsed_us, warn_threshold_us);
    }
    
    // Check for timeout (if enabled)
#if CONFIG_SYSTEM_SERVICE_HANDLER_TIMEOUT_MS > 0
    uint32_t timeout_us = CONFIG_SYSTEM_SERVICE_HANDLER_TIMEOUT_MS * 1000;
    if (elapsed_us > timeout_us) {
        if (service_id < SYSTEM_SERVICE_MAX_SERVICES) {
            g_handler_stats[service_id].timeout_count++;
        }
        ESP_LOGE(TAG, "Handler timeout: service_id=%d, time=%lu us (timeout=%lu us)",
                 service_id, elapsed_us, timeout_us);
        return ESP_ERR_EVENT_HANDLER_TIMEOUT;
    }
#endif
    
    return ESP_OK;
    
#else
    // Monitoring disabled, just execute handler
    handler(event, user_data);
    return ESP_OK;
#endif
}

esp_err_t handler_monitor_get_stats(system_service_id_t service_id,
                                     uint32_t *avg_time_us,
                                     uint32_t *max_time_us,
                                     uint32_t *timeout_count)
{
    if (service_id >= SYSTEM_SERVICE_MAX_SERVICES) {
        return ESP_ERR_INVALID_ARG;
    }
    
    handler_stats_t *stats = &g_handler_stats[service_id];
    
    if (avg_time_us != NULL) {
        if (stats->execution_count > 0) {
            *avg_time_us = (uint32_t)(stats->total_time_us / stats->execution_count);
        } else {
            *avg_time_us = 0;
        }
    }
    
    if (max_time_us != NULL) {
        *max_time_us = stats->max_time_us;
    }
    
    if (timeout_count != NULL) {
        *timeout_count = stats->timeout_count;
    }
    
    return ESP_OK;
}
