#include "system_service/system_service.h"
#include "system_service/memory_utils.h"
#include "system_service/app_symbol_table.h"
#include "system_internal.h"
#include "security.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "system_service";
static system_context_t g_system_ctx = {0};

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
    
    if (xSemaphoreTake(g_system_ctx.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
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

static void system_event_task(void *pvParameters)
{
    system_event_t event;
    
    while (g_system_ctx.running) {
        if (xQueueReceive(g_system_ctx.event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            system_lock();
            
            for (int i = 0; i < SYSTEM_SERVICE_MAX_SUBSCRIBERS; i++) {
                if (g_system_ctx.subscriptions[i].active &&
                    g_system_ctx.subscriptions[i].event_type == event.event_type) {
                    
                    if (g_system_ctx.subscriptions[i].handler) {
                        g_system_ctx.subscriptions[i].handler(&event, 
                                                              g_system_ctx.subscriptions[i].user_data);
                    }
                }
            }
            
            g_system_ctx.total_events_processed++;
            
            if (event.data != NULL) {
                free(event.data);
            }
            
            system_unlock();
        }
    }
    
    vTaskDelete(NULL);
}

esp_err_t system_service_init(system_secure_key_t *out_secure_key)
{
    if (out_secure_key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_system_ctx.initialized) {
        ESP_LOGW(TAG, "System service already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    memset(&g_system_ctx, 0, sizeof(system_context_t));
    
    g_system_ctx.secure_key = security_generate_key();
    *out_secure_key = g_system_ctx.secure_key;
    
    g_system_ctx.mutex = xSemaphoreCreateMutex();
    if (g_system_ctx.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    g_system_ctx.event_queue = xQueueCreate(SYSTEM_EVENT_QUEUE_SIZE, sizeof(system_event_t));
    if (g_system_ctx.event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        vSemaphoreDelete(g_system_ctx.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    g_system_ctx.magic = SYSTEM_MAGIC_NUMBER;
    g_system_ctx.initialized = true;
    g_system_ctx.running = false;
    
    ESP_LOGI(TAG, "System service initialized successfully");
    
    // Initialize symbol table for dynamic apps
    symbol_table_init();
    
    // Log initial memory state
    memory_log_usage(TAG);
    
    return ESP_OK;
}

esp_err_t system_service_deinit(system_secure_key_t secure_key)
{
    if (!system_verify_key(secure_key)) {
        ESP_LOGE(TAG, "Invalid secure key");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_system_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_system_ctx.running) {
        system_service_stop(secure_key);
    }
    
    if (g_system_ctx.event_queue != NULL) {
        vQueueDelete(g_system_ctx.event_queue);
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
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_system_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_system_ctx.running) {
        ESP_LOGW(TAG, "System service already running");
        return ESP_OK;
    }
    
    g_system_ctx.running = true;
    
    BaseType_t ret = xTaskCreate(system_event_task,
                                  "sys_event",
                                  4096,
                                  NULL,
                                  5,
                                  &g_system_ctx.event_task);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create event task");
        g_system_ctx.running = false;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "System service started");
    
    return ESP_OK;
}

esp_err_t system_service_stop(system_secure_key_t secure_key)
{
    if (!system_verify_key(secure_key)) {
        ESP_LOGE(TAG, "Invalid secure key");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_system_ctx.initialized || !g_system_ctx.running) {
        return ESP_ERR_INVALID_STATE;
    }
    
    g_system_ctx.running = false;
    
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
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_system_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
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
