/**
 * @file request_response.c
 * @brief Request/response event pattern implementation
 */

#include "request_response.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "system_service/event_bus.h"
#include <string.h>

static const char *TAG = "request_response";

/* ============================================================================
 * Request Tracking
 * ============================================================================ */

#define MAX_PENDING_REQUESTS 16

typedef struct {
    bool active;
    request_id_t request_id;
    system_service_id_t requester;
    SemaphoreHandle_t response_sem;
    void *response_data;
    size_t response_size;
    size_t response_buffer_size;
    response_callback_t callback;
    void *user_data;
} pending_request_t;

/* ============================================================================
 * Global State
 * ============================================================================ */

static pending_request_t g_pending_requests[MAX_PENDING_REQUESTS] = {0};
static SemaphoreHandle_t g_requests_mutex = NULL;
static uint32_t g_next_request_id = 1;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static void ensure_mutex(void)
{
    if (g_requests_mutex == NULL) {
        g_requests_mutex = xSemaphoreCreateMutex();
    }
}

static request_id_t allocate_request_id(void)
{
    ensure_mutex();
    
    if (xSemaphoreTake(g_requests_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return REQUEST_ID_INVALID;
    }
    
    request_id_t id = g_next_request_id++;
    if (g_next_request_id == REQUEST_ID_INVALID) {
        g_next_request_id = 1;
    }
    
    xSemaphoreGive(g_requests_mutex);
    return id;
}

static pending_request_t* find_request(request_id_t request_id)
{
    for (int i = 0; i < MAX_PENDING_REQUESTS; i++) {
        if (g_pending_requests[i].active && 
            g_pending_requests[i].request_id == request_id) {
            return &g_pending_requests[i];
        }
    }
    return NULL;
}

static pending_request_t* allocate_request_slot(void)
{
    for (int i = 0; i < MAX_PENDING_REQUESTS; i++) {
        if (!g_pending_requests[i].active) {
            return &g_pending_requests[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

esp_err_t request_send_sync(system_service_id_t target_service,
                             system_event_type_t request_type,
                             const void *request_data,
                             size_t request_size,
                             void *response_data,
                             size_t *response_size,
                             uint32_t timeout_ms)
{
    if (response_data == NULL || response_size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ensure_mutex();
    
    // Allocate request ID
    request_id_t request_id = allocate_request_id();
    if (request_id == REQUEST_ID_INVALID) {
        return ESP_ERR_NO_MEM;
    }
    
    // Allocate request slot
    if (xSemaphoreTake(g_requests_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    pending_request_t *req = allocate_request_slot();
    if (req == NULL) {
        xSemaphoreGive(g_requests_mutex);
        ESP_LOGE(TAG, "No request slots available");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize request
    req->active = true;
    req->request_id = request_id;
    req->requester = 0; // TODO: Get current service ID
    req->response_sem = xSemaphoreCreateBinary();
    req->response_data = response_data;
    req->response_buffer_size = *response_size;
    req->response_size = 0;
    req->callback = NULL;
    req->user_data = NULL;
    
    xSemaphoreGive(g_requests_mutex);
    
    // Send request event (TODO: Embed request_id in event data)
    esp_err_t ret = system_event_post(0, request_type, request_data, request_size,
                                      SYSTEM_EVENT_PRIORITY_HIGH);
    if (ret != ESP_OK) {
        vSemaphoreDelete(req->response_sem);
        req->active = false;
        return ret;
    }
    
    // Wait for response
    if (xSemaphoreTake(req->response_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        vSemaphoreDelete(req->response_sem);
        req->active = false;
        ESP_LOGW(TAG, "Request %lu timed out", request_id);
        return ESP_ERR_TIMEOUT;
    }
    
    // Copy response size
    *response_size = req->response_size;
    
    // Cleanup
    vSemaphoreDelete(req->response_sem);
    req->active = false;
    
    ESP_LOGD(TAG, "Request %lu completed", request_id);
    return ESP_OK;
}

esp_err_t request_send_async(system_service_id_t target_service,
                              system_event_type_t request_type,
                              const void *request_data,
                              size_t request_size,
                              response_callback_t callback,
                              void *user_data,
                              request_id_t *out_request_id)
{
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ensure_mutex();
    
    // Allocate request ID
    request_id_t request_id = allocate_request_id();
    if (request_id == REQUEST_ID_INVALID) {
        return ESP_ERR_NO_MEM;
    }
    
    // Allocate request slot
    if (xSemaphoreTake(g_requests_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    pending_request_t *req = allocate_request_slot();
    if (req == NULL) {
        xSemaphoreGive(g_requests_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize request
    req->active = true;
    req->request_id = request_id;
    req->requester = 0;
    req->response_sem = NULL;
    req->callback = callback;
    req->user_data = user_data;
    
    xSemaphoreGive(g_requests_mutex);
    
    // Send request event
    esp_err_t ret = system_event_post(0, request_type, request_data, request_size,
                                      SYSTEM_EVENT_PRIORITY_HIGH);
    if (ret != ESP_OK) {
        req->active = false;
        return ret;
    }
    
    if (out_request_id != NULL) {
        *out_request_id = request_id;
    }
    
    return ESP_OK;
}

esp_err_t request_send_response(request_id_t request_id,
                                 const void *response_data,
                                 size_t response_size)
{
    ensure_mutex();
    
    if (xSemaphoreTake(g_requests_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    pending_request_t *req = find_request(request_id);
    if (req == NULL) {
        xSemaphoreGive(g_requests_mutex);
        ESP_LOGW(TAG, "Request %lu not found", request_id);
        return ESP_ERR_NOT_FOUND;
    }
    
    if (req->callback != NULL) {
        // Async request - call callback
        xSemaphoreGive(g_requests_mutex);
        req->callback(request_id, response_data, response_size, req->user_data);
        
        if (xSemaphoreTake(g_requests_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return ESP_OK;
        }
        req->active = false;
        xSemaphoreGive(g_requests_mutex);
    } else {
        // Sync request - copy data and signal semaphore
        if (req->response_data != NULL && response_data != NULL) {
            size_t copy_size = (response_size < req->response_buffer_size) ? 
                               response_size : req->response_buffer_size;
            memcpy(req->response_data, response_data, copy_size);
            req->response_size = copy_size;
        }
        
        xSemaphoreGive(g_requests_mutex);
        
        if (req->response_sem != NULL) {
            xSemaphoreGive(req->response_sem);
        }
    }
    
    return ESP_OK;
}

esp_err_t request_cancel(request_id_t request_id)
{
    ensure_mutex();
    
    if (xSemaphoreTake(g_requests_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    pending_request_t *req = find_request(request_id);
    if (req == NULL) {
        xSemaphoreGive(g_requests_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    if (req->response_sem != NULL) {
        vSemaphoreDelete(req->response_sem);
    }
    
    req->active = false;
    
    xSemaphoreGive(g_requests_mutex);
    
    ESP_LOGI(TAG, "Request %lu cancelled", request_id);
    return ESP_OK;
}
