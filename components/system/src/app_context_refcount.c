/**
 * @file app_context_refcount.c
 * @brief App context reference counting implementation
 */

#include "app_context_refcount.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "system_service/error_codes.h"

static const char *TAG = "app_context_refcount";

/* ============================================================================
 * Context Tracking
 * ============================================================================ */

typedef struct {
    system_service_id_t service_id;
    uint32_t refcount;
    bool valid;
    bool marked_for_deletion;
} context_refcount_t;

static context_refcount_t g_contexts[SYSTEM_SERVICE_MAX_SERVICES] = {0};
static SemaphoreHandle_t g_context_mutex = NULL;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static void ensure_mutex(void)
{
    if (g_context_mutex == NULL) {
        g_context_mutex = xSemaphoreCreateMutex();
    }
}

static context_refcount_t* find_context(system_service_id_t service_id)
{
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (g_contexts[i].valid && g_contexts[i].service_id == service_id) {
            return &g_contexts[i];
        }
    }
    return NULL;
}

static context_refcount_t* get_or_create_context(system_service_id_t service_id)
{
    context_refcount_t *ctx = find_context(service_id);
    if (ctx != NULL) {
        return ctx;
    }
    
    // Find empty slot
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (!g_contexts[i].valid) {
            g_contexts[i].service_id = service_id;
            g_contexts[i].refcount = 0;
            g_contexts[i].valid = true;
            g_contexts[i].marked_for_deletion = false;
            return &g_contexts[i];
        }
    }
    
    return NULL;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

esp_err_t app_context_acquire(system_service_id_t service_id)
{
    ensure_mutex();
    
    if (xSemaphoreTake(g_context_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    context_refcount_t *ctx = get_or_create_context(service_id);
    if (ctx == NULL) {
        xSemaphoreGive(g_context_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    if (!ctx->valid || ctx->marked_for_deletion) {
        xSemaphoreGive(g_context_mutex);
        return ESP_ERR_APP_CONTEXT_INVALID;
    }
    
    ctx->refcount++;
    
    xSemaphoreGive(g_context_mutex);
    
    ESP_LOGD(TAG, "Acquired context for service %d (refcount=%lu)", 
             service_id, ctx->refcount);
    
    return ESP_OK;
}

esp_err_t app_context_release(system_service_id_t service_id)
{
    ensure_mutex();
    
    if (xSemaphoreTake(g_context_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    context_refcount_t *ctx = find_context(service_id);
    if (ctx == NULL) {
        xSemaphoreGive(g_context_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    if (ctx->refcount > 0) {
        ctx->refcount--;
        
        ESP_LOGD(TAG, "Released context for service %d (refcount=%lu)", 
                 service_id, ctx->refcount);
        
        // Check if should delete
        if (ctx->refcount == 0 && ctx->marked_for_deletion) {
            ESP_LOGI(TAG, "Deleting context for service %d (refcount reached 0)", 
                     service_id);
            ctx->valid = false;
            ctx->marked_for_deletion = false;
        }
    }
    
    xSemaphoreGive(g_context_mutex);
    
    return ESP_OK;
}

esp_err_t app_context_get_refcount(system_service_id_t service_id, uint32_t *count)
{
    if (count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ensure_mutex();
    
    if (xSemaphoreTake(g_context_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    context_refcount_t *ctx = find_context(service_id);
    *count = (ctx != NULL) ? ctx->refcount : 0;
    
    xSemaphoreGive(g_context_mutex);
    
    return ESP_OK;
}

bool app_context_is_valid(system_service_id_t service_id)
{
    ensure_mutex();
    
    if (xSemaphoreTake(g_context_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    
    context_refcount_t *ctx = find_context(service_id);
    bool valid = (ctx != NULL && ctx->valid && !ctx->marked_for_deletion);
    
    xSemaphoreGive(g_context_mutex);
    
    return valid;
}

esp_err_t app_context_mark_for_deletion(system_service_id_t service_id)
{
    ensure_mutex();
    
    if (xSemaphoreTake(g_context_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    context_refcount_t *ctx = find_context(service_id);
    if (ctx == NULL) {
        xSemaphoreGive(g_context_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    ctx->marked_for_deletion = true;
    
    ESP_LOGI(TAG, "Marked context for deletion: service %d (refcount=%lu)", 
             service_id, ctx->refcount);
    
    // Delete immediately if no references
    if (ctx->refcount == 0) {
        ESP_LOGI(TAG, "Deleting context immediately (no references)");
        ctx->valid = false;
        ctx->marked_for_deletion = false;
    }
    
    xSemaphoreGive(g_context_mutex);
    
    return ESP_OK;
}
