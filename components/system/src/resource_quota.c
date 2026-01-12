/**
 * @file resource_quota.c
 * @brief Resource quota implementation
 */

#include "resource_quota.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "system_service/error_codes.h"
#include <string.h>

static const char *TAG = "quota";

/* ============================================================================
 * Quota Entry Structure
 * ============================================================================ */

typedef struct {
    bool active;                        /**< Entry is active */
    system_service_id_t service_id;     /**< Service ID */
    service_quota_t quota;              /**< Quota limits */
    service_quota_usage_t usage;        /**< Current usage */
    uint32_t last_reset_time;           /**< Last counter reset time */
} quota_entry_t;

/* ============================================================================
 * Global State
 * ============================================================================ */

typedef struct {
    bool initialized;                   /**< System initialized */
    SemaphoreHandle_t mutex;            /**< Mutex for thread safety */
    quota_entry_t entries[SYSTEM_SERVICE_MAX_SERVICES];
    service_quota_t default_quota;      /**< Default quota values */
} quota_context_t;

static quota_context_t g_quota_ctx = {0};

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static inline uint32_t get_time_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static quota_entry_t* find_entry(system_service_id_t service_id)
{
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (g_quota_ctx.entries[i].active && 
            g_quota_ctx.entries[i].service_id == service_id) {
            return &g_quota_ctx.entries[i];
        }
    }
    return NULL;
}

static quota_entry_t* allocate_entry(system_service_id_t service_id)
{
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (!g_quota_ctx.entries[i].active) {
            g_quota_ctx.entries[i].active = true;
            g_quota_ctx.entries[i].service_id = service_id;
            return &g_quota_ctx.entries[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

esp_err_t quota_init(void)
{
    if (g_quota_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing quota system...");
    
    // Create mutex
    g_quota_ctx.mutex = xSemaphoreCreateMutex();
    if (g_quota_ctx.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create quota mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Set default quotas from config
    g_quota_ctx.default_quota.max_events_per_sec = CONFIG_SYSTEM_SERVICE_DEFAULT_EVENT_QUOTA_PER_SEC;
    g_quota_ctx.default_quota.max_subscriptions = CONFIG_SYSTEM_SERVICE_DEFAULT_SUBSCRIPTION_QUOTA;
    g_quota_ctx.default_quota.max_event_data_size = CONFIG_SYSTEM_SERVICE_MAX_DATA_SIZE;
    g_quota_ctx.default_quota.max_memory_bytes = CONFIG_SYSTEM_SERVICE_DEFAULT_MEMORY_QUOTA_KB * 1024;
    
    // Initialize entries
    memset(g_quota_ctx.entries, 0, sizeof(g_quota_ctx.entries));
    
    g_quota_ctx.initialized = true;
    
    ESP_LOGI(TAG, "Quota system initialized");
    ESP_LOGI(TAG, "  Default event quota: %lu/sec", g_quota_ctx.default_quota.max_events_per_sec);
    ESP_LOGI(TAG, "  Default subscription quota: %lu", g_quota_ctx.default_quota.max_subscriptions);
    ESP_LOGI(TAG, "  Default memory quota: %lu KB", CONFIG_SYSTEM_SERVICE_DEFAULT_MEMORY_QUOTA_KB);
    
    return ESP_OK;
}

esp_err_t quota_deinit(void)
{
    if (!g_quota_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing quota system...");
    
    if (g_quota_ctx.mutex != NULL) {
        vSemaphoreDelete(g_quota_ctx.mutex);
        g_quota_ctx.mutex = NULL;
    }
    
    g_quota_ctx.initialized = false;
    
    ESP_LOGI(TAG, "Quota system deinitialized");
    return ESP_OK;
}

esp_err_t quota_set(system_service_id_t service_id, const service_quota_t *quota)
{
    if (!g_quota_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_quota_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    quota_entry_t *entry = find_entry(service_id);
    if (entry == NULL) {
        entry = allocate_entry(service_id);
        if (entry == NULL) {
            xSemaphoreGive(g_quota_ctx.mutex);
            return ESP_ERR_NO_MEM;
        }
        
        // Initialize usage
        memset(&entry->usage, 0, sizeof(service_quota_usage_t));
        entry->last_reset_time = get_time_ms();
    }
    
    // Set quota (use defaults if NULL)
    if (quota != NULL) {
        entry->quota = *quota;
    } else {
        entry->quota = g_quota_ctx.default_quota;
    }
    
    xSemaphoreGive(g_quota_ctx.mutex);
    
    ESP_LOGI(TAG, "Quota set for service %d", service_id);
    return ESP_OK;
}

esp_err_t quota_get(system_service_id_t service_id, service_quota_t *quota)
{
    if (!g_quota_ctx.initialized || quota == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_quota_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    quota_entry_t *entry = find_entry(service_id);
    if (entry == NULL) {
        *quota = g_quota_ctx.default_quota;
    } else {
        *quota = entry->quota;
    }
    
    xSemaphoreGive(g_quota_ctx.mutex);
    
    return ESP_OK;
}

esp_err_t quota_get_usage(system_service_id_t service_id, service_quota_usage_t *usage)
{
    if (!g_quota_ctx.initialized || usage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_quota_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    quota_entry_t *entry = find_entry(service_id);
    if (entry == NULL) {
        memset(usage, 0, sizeof(service_quota_usage_t));
    } else {
        *usage = entry->usage;
    }
    
    xSemaphoreGive(g_quota_ctx.mutex);
    
    return ESP_OK;
}

esp_err_t quota_check_event_post(system_service_id_t service_id)
{
    if (!g_quota_ctx.initialized) {
        return ESP_OK; // Quotas disabled
    }
    
    if (xSemaphoreTake(g_quota_ctx.mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_OK; // Don't block on quota check
    }
    
    quota_entry_t *entry = find_entry(service_id);
    if (entry == NULL) {
        xSemaphoreGive(g_quota_ctx.mutex);
        return ESP_OK; // No quota set
    }
    
    // Check event rate quota
    if (entry->usage.events_this_sec >= entry->quota.max_events_per_sec) {
        entry->usage.quota_violations++;
        xSemaphoreGive(g_quota_ctx.mutex);
        ESP_LOGW(TAG, "Service %d exceeded event quota (%lu/%lu)",
                 service_id, entry->usage.events_this_sec, entry->quota.max_events_per_sec);
        return ESP_ERR_QUOTA_EVENTS_EXCEEDED;
    }
    
    xSemaphoreGive(g_quota_ctx.mutex);
    return ESP_OK;
}

esp_err_t quota_record_event_post(system_service_id_t service_id)
{
    if (!g_quota_ctx.initialized) {
        return ESP_OK;
    }
    
    if (xSemaphoreTake(g_quota_ctx.mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_OK;
    }
    
    quota_entry_t *entry = find_entry(service_id);
    if (entry != NULL) {
        entry->usage.events_this_sec++;
        entry->usage.total_events_posted++;
    }
    
    xSemaphoreGive(g_quota_ctx.mutex);
    return ESP_OK;
}

esp_err_t quota_check_subscription(system_service_id_t service_id)
{
    if (!g_quota_ctx.initialized) {
        return ESP_OK;
    }
    
    if (xSemaphoreTake(g_quota_ctx.mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_OK;
    }
    
    quota_entry_t *entry = find_entry(service_id);
    if (entry == NULL) {
        xSemaphoreGive(g_quota_ctx.mutex);
        return ESP_OK;
    }
    
    if (entry->usage.active_subscriptions >= entry->quota.max_subscriptions) {
        entry->usage.quota_violations++;
        xSemaphoreGive(g_quota_ctx.mutex);
        ESP_LOGW(TAG, "Service %d exceeded subscription quota (%lu/%lu)",
                 service_id, entry->usage.active_subscriptions, entry->quota.max_subscriptions);
        return ESP_ERR_QUOTA_SUBSCRIPTIONS_EXCEEDED;
    }
    
    xSemaphoreGive(g_quota_ctx.mutex);
    return ESP_OK;
}

esp_err_t quota_record_subscription(system_service_id_t service_id, bool add)
{
    if (!g_quota_ctx.initialized) {
        return ESP_OK;
    }
    
    if (xSemaphoreTake(g_quota_ctx.mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_OK;
    }
    
    quota_entry_t *entry = find_entry(service_id);
    if (entry != NULL) {
        if (add) {
            entry->usage.active_subscriptions++;
        } else if (entry->usage.active_subscriptions > 0) {
            entry->usage.active_subscriptions--;
        }
    }
    
    xSemaphoreGive(g_quota_ctx.mutex);
    return ESP_OK;
}

esp_err_t quota_check_data_size(system_service_id_t service_id, size_t data_size)
{
    if (!g_quota_ctx.initialized) {
        return ESP_OK;
    }
    
    if (xSemaphoreTake(g_quota_ctx.mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_OK;
    }
    
    quota_entry_t *entry = find_entry(service_id);
    if (entry == NULL) {
        xSemaphoreGive(g_quota_ctx.mutex);
        return ESP_OK;
    }
    
    if (data_size > entry->quota.max_event_data_size) {
        entry->usage.quota_violations++;
        xSemaphoreGive(g_quota_ctx.mutex);
        ESP_LOGW(TAG, "Service %d exceeded data size quota (%zu/%lu)",
                 service_id, data_size, entry->quota.max_event_data_size);
        return ESP_ERR_QUOTA_DATA_SIZE_EXCEEDED;
    }
    
    xSemaphoreGive(g_quota_ctx.mutex);
    return ESP_OK;
}

esp_err_t quota_record_memory_alloc(system_service_id_t service_id, size_t size)
{
    if (!g_quota_ctx.initialized) {
        return ESP_OK;
    }
    
    if (xSemaphoreTake(g_quota_ctx.mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_OK;
    }
    
    quota_entry_t *entry = find_entry(service_id);
    if (entry != NULL) {
        entry->usage.current_memory_bytes += size;
        
        // Check quota
        if (entry->usage.current_memory_bytes > entry->quota.max_memory_bytes) {
            ESP_LOGW(TAG, "Service %d exceeded memory quota (%lu/%lu)",
                     service_id, entry->usage.current_memory_bytes, entry->quota.max_memory_bytes);
        }
    }
    
    xSemaphoreGive(g_quota_ctx.mutex);
    return ESP_OK;
}

esp_err_t quota_record_memory_free(system_service_id_t service_id, size_t size)
{
    if (!g_quota_ctx.initialized) {
        return ESP_OK;
    }
    
    if (xSemaphoreTake(g_quota_ctx.mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return ESP_OK;
    }
    
    quota_entry_t *entry = find_entry(service_id);
    if (entry != NULL) {
        if (entry->usage.current_memory_bytes >= size) {
            entry->usage.current_memory_bytes -= size;
        } else {
            entry->usage.current_memory_bytes = 0;
        }
    }
    
    xSemaphoreGive(g_quota_ctx.mutex);
    return ESP_OK;
}

esp_err_t quota_reset_counters(void)
{
    if (!g_quota_ctx.initialized) {
        return ESP_OK;
    }
    
    if (xSemaphoreTake(g_quota_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    uint32_t now = get_time_ms();
    
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (g_quota_ctx.entries[i].active) {
            // Reset per-second counters
            g_quota_ctx.entries[i].usage.events_this_sec = 0;
            g_quota_ctx.entries[i].last_reset_time = now;
        }
    }
    
    xSemaphoreGive(g_quota_ctx.mutex);
    
    return ESP_OK;
}

void quota_log_status(const char *tag)
{
    if (!g_quota_ctx.initialized) {
        ESP_LOGW(tag, "Quota system not initialized");
        return;
    }
    
    ESP_LOGI(tag, "Quota Status:");
    ESP_LOGI(tag, "  Service | Events/s | Subs | Memory | Violations");
    ESP_LOGI(tag, "  --------|----------|------|--------|------------");
    
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (g_quota_ctx.entries[i].active) {
            quota_entry_t *entry = &g_quota_ctx.entries[i];
            ESP_LOGI(tag, "  %7d | %4lu/%3lu | %2lu/%2lu | %4lu/%4lu | %10lu",
                     entry->service_id,
                     entry->usage.events_this_sec,
                     entry->quota.max_events_per_sec,
                     entry->usage.active_subscriptions,
                     entry->quota.max_subscriptions,
                     entry->usage.current_memory_bytes / 1024,
                     entry->quota.max_memory_bytes / 1024,
                     entry->usage.quota_violations);
        }
    }
}
