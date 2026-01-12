/**
 * @file system_service.c
 * @brief System service implementation with production features
 */

#include "system_service/system_service.h"
#include "system_service/memory_utils.h"
#include "system_service/error_codes.h"
#include "system_internal.h"
#include "security.h"
#include "memory_pool.h"
#include "priority_queue.h"
#include "resource_quota.h"
#include "service_watchdog.h"
#include "service_dependencies.h"
#include "handler_monitor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "system_service";
static system_context_t g_system_ctx = {0};

/* Event processing task with production features */
static void event_task(void *arg)
{
    system_context_t *ctx = (system_context_t *)arg;
    system_event_t event;
    
    ESP_LOGI(TAG, "Event processing task started");
    
    while (ctx->running) {
        // Receive from priority queue (handles priority automatically)
        esp_err_t ret = priority_queue_receive(ctx->event_queue, &event, portMAX_DELAY);
        if (ret != ESP_OK) {
            continue;
        }
        
        // Process event
        ret = system_lock();
        if (ret != ESP_OK) {
            if (event.data != NULL) {
                memory_pool_free(event.data);
            }
            continue;
        }
        
        // Find and call all subscribers
        for (int i = 0; i < SYSTEM_SERVICE_MAX_SUBSCRIBERS; i++) {
            if (ctx->subscriptions[i].active &&
                ctx->subscriptions[i].event_type == event.event_type) {
                
                system_service_id_t subscriber_id = ctx->subscriptions[i].service_id;
                system_event_handler_t handler = ctx->subscriptions[i].handler;
                void *user_data = ctx->subscriptions[i].user_data;
                
                system_unlock();
                
                // Execute handler with monitoring (if enabled)
#if CONFIG_SYSTEM_SERVICE_ENABLE_HANDLER_MONITORING
                esp_err_t handler_ret = handler_monitor_execute(
                    handler,
                    &event,
                    user_data,
                    subscriber_id
                );
                
                if (handler_ret != ESP_OK) {
                    ESP_LOGW(TAG, "Handler for service %d returned error: %s",
                             subscriber_id, system_service_err_to_name(handler_ret));
                }
#else
                // Direct handler call without monitoring
                handler(&event, user_data);
#endif
                
                ret = system_lock();
                if (ret != ESP_OK) {
                    break;
                }
            }
        }
        
        ctx->total_events_processed++;
        
        system_unlock();
        
        // Free event data using memory pool
        if (event.data != NULL) {
            memory_pool_free(event.data);
        }
    }
    
    ESP_LOGI(TAG, "Event processing task stopped");
    vTaskDelete(NULL);
}

system_context_t* system_get_context(void)
{
    return &g_system_ctx;
}

bool system_verify_key(system_secure_key_t key)
{
    if (!g_system_ctx.initialized) {
        return false;
    }
    return security_validate_key(key, g_system_ctx.secure_key);
}

esp_err_t system_lock(void)
{
    if (!g_system_ctx.initialized || g_system_ctx.mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_system_ctx.mutex, pdMS_TO_TICKS(SYSTEM_SERVICE_MUTEX_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

esp_err_t system_unlock(void)
{
    if (!g_system_ctx.initialized || g_system_ctx.mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreGive(g_system_ctx.mutex);
    return ESP_OK;
}

esp_err_t system_service_init(system_secure_key_t *out_secure_key)
{
    if (out_secure_key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_system_ctx.initialized) {
        ESP_LOGW(TAG, "System service already initialized");
        return ESP_ERR_SYSTEM_ALREADY_INITIALIZED;
    }
    
    ESP_LOGI(TAG, "Initializing system service...");
    
    memset(&g_system_ctx, 0, sizeof(system_context_t));
    
    // Generate secure key
    g_system_ctx.secure_key = security_generate_key();
    *out_secure_key = g_system_ctx.secure_key;
    
    // Create mutex
    g_system_ctx.mutex = xSemaphoreCreateMutex();
    if (g_system_ctx.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Create priority queue instead of simple queue
    esp_err_t ret = priority_queue_create(&g_system_ctx.event_queue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create priority queue: %s", system_service_err_to_name(ret));
        vSemaphoreDelete(g_system_ctx.mutex);
        return ret;
    }
    
    // Initialize production systems
    ESP_LOGI(TAG, "Initializing production systems...");
    
    // Memory pools
    ret = memory_pool_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize memory pools: %s", system_service_err_to_name(ret));
        priority_queue_destroy(g_system_ctx.event_queue);
        vSemaphoreDelete(g_system_ctx.mutex);
        return ret;
    }
    
    // Resource quotas
    ret = quota_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize quotas: %s", system_service_err_to_name(ret));
        // Continue anyway
    }
    
    // Service dependencies
    ret = dependencies_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize dependencies: %s", system_service_err_to_name(ret));
        // Continue anyway
    }
    
    // Watchdog
    ret = watchdog_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize watchdog: %s", system_service_err_to_name(ret));
        // Continue anyway
    }
    
    g_system_ctx.magic = SYSTEM_MAGIC_NUMBER;
    g_system_ctx.initialized = true;
    g_system_ctx.running = false;
    
    ESP_LOGI(TAG, "System service initialized successfully");
    
    // Log initial memory state
    memory_log_usage(TAG);
    
    return ESP_OK;
}

esp_err_t system_service_deinit(system_secure_key_t secure_key)
{
    if (!system_verify_key(secure_key)) {
        ESP_LOGE(TAG, "Invalid secure key");
        return ESP_ERR_SECURITY_INVALID_KEY;
    }
    
    if (!g_system_ctx.initialized) {
        return ESP_ERR_SYSTEM_NOT_INITIALIZED;
    }
    
    ESP_LOGI(TAG, "Deinitializing system service...");
    
    if (g_system_ctx.running) {
        system_service_stop(secure_key);
    }
    
    // Deinitialize production systems
    watchdog_deinit();
    dependencies_deinit();
    quota_deinit();
    memory_pool_deinit();
    
    if (g_system_ctx.event_queue != NULL) {
        priority_queue_destroy(g_system_ctx.event_queue);
    }
    
    if (g_system_ctx.mutex != NULL) {
        vSemaphoreDelete(g_system_ctx.mutex);
    }
    
    security_invalidate_key(&g_system_ctx.secure_key);
    memset(&g_system_ctx, 0, sizeof(system_context_t));
    
    ESP_LOGI(TAG, "System service deinitialized");
    
    return ESP_OK;
}

esp_err_t system_service_start(system_secure_key_t secure_key)
{
    if (!system_verify_key(secure_key)) {
        ESP_LOGE(TAG, "Invalid secure key");
        return ESP_ERR_SECURITY_INVALID_KEY;
    }
    
    if (!g_system_ctx.initialized) {
        return ESP_ERR_SYSTEM_NOT_INITIALIZED;
    }
    
    if (g_system_ctx.running) {
        ESP_LOGW(TAG, "System service already running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting system service...");
    
    g_system_ctx.running = true;
    
    // Start watchdog monitoring
    esp_err_t ret = watchdog_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start watchdog: %s", system_service_err_to_name(ret));
    }
    
    // Create event processing task (defined in event_bus.c)
    BaseType_t task_ret = xTaskCreate(event_task,
                                      "sys_event",
                                      4096,
                                      &g_system_ctx,
                                      5,
                                      &g_system_ctx.event_task);
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create event task");
        g_system_ctx.running = false;
        watchdog_stop();
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "System service started");
    
    return ESP_OK;
}

esp_err_t system_service_stop(system_secure_key_t secure_key)
{
    if (!system_verify_key(secure_key)) {
        ESP_LOGE(TAG, "Invalid secure key");
        return ESP_ERR_SECURITY_INVALID_KEY;
    }
    
    if (!g_system_ctx.initialized || !g_system_ctx.running) {
        return ESP_ERR_SYSTEM_NOT_STARTED;
    }
    
    ESP_LOGI(TAG, "Stopping system service...");
    
    g_system_ctx.running = false;
    
    // Stop watchdog
    watchdog_stop();
    
    if (g_system_ctx.event_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(200));
        g_system_ctx.event_task = NULL;
    }
    
    ESP_LOGI(TAG, "System service stopped");
    
    return ESP_OK;
}

esp_err_t system_service_get_stats(system_secure_key_t secure_key,
                                   uint32_t *out_total_services,
                                   uint32_t *out_total_events,
                                   uint32_t *out_total_subscriptions)
{
    if (!system_verify_key(secure_key)) {
        ESP_LOGE(TAG, "Invalid secure key");
        return ESP_ERR_SECURITY_INVALID_KEY;
    }
    
    if (!g_system_ctx.initialized) {
        return ESP_ERR_SYSTEM_NOT_INITIALIZED;
    }
    
    esp_err_t ret = system_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (out_total_services != NULL) {
        *out_total_services = g_system_ctx.service_count;
    }
    
    if (out_total_events != NULL) {
        *out_total_events = g_system_ctx.total_events_processed;
    }
    
    if (out_total_subscriptions != NULL) {
        *out_total_subscriptions = g_system_ctx.subscription_count;
    }
    
    system_unlock();
    
    return ESP_OK;
}
