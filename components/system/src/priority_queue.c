/**
 * @file priority_queue.c
 * @brief Priority-based event queue implementation
 */

#include "priority_queue.h"
#include "memory_pool.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "priority_queue";

/* ============================================================================
 * Queue Structure
 * ============================================================================ */

struct priority_queue {
    QueueHandle_t queues[4];        /**< Queues for each priority level */
    SemaphoreHandle_t mutex;        /**< Mutex for statistics */
    event_queue_stats_t stats;      /**< Queue statistics */
    uint32_t sequence_counter;      /**< Event sequence counter */
};

/* Queue sizes from config */
static const uint32_t queue_sizes[] = {
    [SYSTEM_EVENT_PRIORITY_LOW] = CONFIG_SYSTEM_SERVICE_LOW_PRIORITY_QUEUE_SIZE,
    [SYSTEM_EVENT_PRIORITY_NORMAL] = CONFIG_SYSTEM_SERVICE_NORMAL_PRIORITY_QUEUE_SIZE,
    [SYSTEM_EVENT_PRIORITY_HIGH] = CONFIG_SYSTEM_SERVICE_HIGH_PRIORITY_QUEUE_SIZE,
    [SYSTEM_EVENT_PRIORITY_CRITICAL] = CONFIG_SYSTEM_SERVICE_HIGH_PRIORITY_QUEUE_SIZE,
};

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

esp_err_t priority_queue_create(priority_queue_handle_t *handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Allocate queue structure
    struct priority_queue *pq = calloc(1, sizeof(struct priority_queue));
    if (pq == NULL) {
        ESP_LOGE(TAG, "Failed to allocate priority queue");
        return ESP_ERR_NO_MEM;
    }
    
    // Create mutex
    pq->mutex = xSemaphoreCreateMutex();
    if (pq->mutex == NULL) {
        free(pq);
        return ESP_ERR_NO_MEM;
    }
    
    // Create queues for each priority level
    bool success = true;
    for (int i = 0; i < 4; i++) {
        pq->queues[i] = xQueueCreate(queue_sizes[i], sizeof(system_event_t));
        if (pq->queues[i] == NULL) {
            ESP_LOGE(TAG, "Failed to create queue for priority %d", i);
            success = false;
            break;
        }
    }
    
    if (!success) {
        // Cleanup on failure
        for (int i = 0; i < 4; i++) {
            if (pq->queues[i] != NULL) {
                vQueueDelete(pq->queues[i]);
            }
        }
        vSemaphoreDelete(pq->mutex);
        free(pq);
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize statistics
    memset(&pq->stats, 0, sizeof(event_queue_stats_t));
    pq->sequence_counter = 0;
    
    *handle = pq;
    
    ESP_LOGI(TAG, "Priority queue created (HIGH=%lu, NORMAL=%lu, LOW=%lu)",
             queue_sizes[SYSTEM_EVENT_PRIORITY_HIGH],
             queue_sizes[SYSTEM_EVENT_PRIORITY_NORMAL],
             queue_sizes[SYSTEM_EVENT_PRIORITY_LOW]);
    
    return ESP_OK;
}

esp_err_t priority_queue_destroy(priority_queue_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct priority_queue *pq = (struct priority_queue *)handle;
    
    // Delete all queues
    for (int i = 0; i < 4; i++) {
        if (pq->queues[i] != NULL) {
            // Free any remaining events
            system_event_t event;
            while (xQueueReceive(pq->queues[i], &event, 0) == pdTRUE) {
                if (event.data != NULL) {
                    memory_pool_free(event.data);
                }
            }
            vQueueDelete(pq->queues[i]);
        }
    }
    
    if (pq->mutex != NULL) {
        vSemaphoreDelete(pq->mutex);
    }
    
    free(pq);
    
    ESP_LOGI(TAG, "Priority queue destroyed");
    return ESP_OK;
}

esp_err_t priority_queue_post(priority_queue_handle_t handle,
                               const system_event_t *event,
                               uint32_t timeout_ms)
{
    if (handle == NULL || event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (event->priority >= 4) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct priority_queue *pq = (struct priority_queue *)handle;
    
    // Copy event
    system_event_t event_copy = *event;
    
    // Assign sequence number
    if (xSemaphoreTake(pq->mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        event_copy.sequence_number = pq->sequence_counter++;
        pq->stats.total_events_queued++;
        xSemaphoreGive(pq->mutex);
    }
    
    // Post to appropriate queue
    QueueHandle_t queue = pq->queues[event->priority];
    BaseType_t ret = xQueueSend(queue, &event_copy, pdMS_TO_TICKS(timeout_ms));
    
    if (ret != pdTRUE) {
        // Queue full - handle overflow
        if (xSemaphoreTake(pq->mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            switch (event->priority) {
                case SYSTEM_EVENT_PRIORITY_HIGH:
                case SYSTEM_EVENT_PRIORITY_CRITICAL:
                    pq->stats.high_priority_overflows++;
                    break;
                case SYSTEM_EVENT_PRIORITY_NORMAL:
                    pq->stats.normal_priority_overflows++;
                    break;
                case SYSTEM_EVENT_PRIORITY_LOW:
                    pq->stats.low_priority_overflows++;
                    // Try to drop oldest low priority event
                    system_event_t dropped;
                    if (xQueueReceive(pq->queues[SYSTEM_EVENT_PRIORITY_LOW], &dropped, 0) == pdTRUE) {
                        if (dropped.data != NULL) {
                            memory_pool_free(dropped.data);
                        }
                        pq->stats.low_priority_drops++;
                        // Try posting again
                        if (xQueueSend(queue, &event_copy, 0) == pdTRUE) {
                            xSemaphoreGive(pq->mutex);
                            ESP_LOGW(TAG, "Dropped low priority event to make room");
                            return ESP_OK;
                        }
                    }
                    break;
            }
            xSemaphoreGive(pq->mutex);
        }
        
        ESP_LOGW(TAG, "Queue full for priority %d", event->priority);
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

esp_err_t priority_queue_receive(priority_queue_handle_t handle,
                                  system_event_t *event,
                                  uint32_t timeout_ms)
{
    if (handle == NULL || event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct priority_queue *pq = (struct priority_queue *)handle;
    
    // Try to receive from queues in priority order
    // CRITICAL/HIGH first, then NORMAL, then LOW
    const int priority_order[] = {
        SYSTEM_EVENT_PRIORITY_CRITICAL,
        SYSTEM_EVENT_PRIORITY_HIGH,
        SYSTEM_EVENT_PRIORITY_NORMAL,
        SYSTEM_EVENT_PRIORITY_LOW
    };
    
    TickType_t start_ticks = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    
    while (1) {
        // Try each queue in priority order
        for (int i = 0; i < 4; i++) {
            int priority = priority_order[i];
            if (xQueueReceive(pq->queues[priority], event, 0) == pdTRUE) {
                // Update statistics
                if (xSemaphoreTake(pq->mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    pq->stats.total_events_processed++;
                    
                    // Update queue depths
                    pq->stats.high_priority_depth = uxQueueMessagesWaiting(pq->queues[SYSTEM_EVENT_PRIORITY_HIGH]) +
                                                    uxQueueMessagesWaiting(pq->queues[SYSTEM_EVENT_PRIORITY_CRITICAL]);
                    pq->stats.normal_priority_depth = uxQueueMessagesWaiting(pq->queues[SYSTEM_EVENT_PRIORITY_NORMAL]);
                    pq->stats.low_priority_depth = uxQueueMessagesWaiting(pq->queues[SYSTEM_EVENT_PRIORITY_LOW]);
                    
                    xSemaphoreGive(pq->mutex);
                }
                
                return ESP_OK;
            }
        }
        
        // Check timeout
        TickType_t elapsed = xTaskGetTickCount() - start_ticks;
        if (elapsed >= timeout_ticks) {
            return ESP_ERR_TIMEOUT;
        }
        
        // Wait a bit before trying again
        vTaskDelay(1);
    }
}

esp_err_t priority_queue_get_stats(priority_queue_handle_t handle,
                                    event_queue_stats_t *stats)
{
    if (handle == NULL || stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct priority_queue *pq = (struct priority_queue *)handle;
    
    if (xSemaphoreTake(pq->mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Update current depths
    pq->stats.high_priority_depth = uxQueueMessagesWaiting(pq->queues[SYSTEM_EVENT_PRIORITY_HIGH]) +
                                    uxQueueMessagesWaiting(pq->queues[SYSTEM_EVENT_PRIORITY_CRITICAL]);
    pq->stats.normal_priority_depth = uxQueueMessagesWaiting(pq->queues[SYSTEM_EVENT_PRIORITY_NORMAL]);
    pq->stats.low_priority_depth = uxQueueMessagesWaiting(pq->queues[SYSTEM_EVENT_PRIORITY_LOW]);
    
    memcpy(stats, &pq->stats, sizeof(event_queue_stats_t));
    
    xSemaphoreGive(pq->mutex);
    
    return ESP_OK;
}

esp_err_t priority_queue_reset_stats(priority_queue_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct priority_queue *pq = (struct priority_queue *)handle;
    
    if (xSemaphoreTake(pq->mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memset(&pq->stats, 0, sizeof(event_queue_stats_t));
    
    xSemaphoreGive(pq->mutex);
    
    return ESP_OK;
}

esp_err_t priority_queue_get_depths(priority_queue_handle_t handle,
                                     uint32_t *high,
                                     uint32_t *normal,
                                     uint32_t *low)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    struct priority_queue *pq = (struct priority_queue *)handle;
    
    if (high != NULL) {
        *high = uxQueueMessagesWaiting(pq->queues[SYSTEM_EVENT_PRIORITY_HIGH]) +
                uxQueueMessagesWaiting(pq->queues[SYSTEM_EVENT_PRIORITY_CRITICAL]);
    }
    
    if (normal != NULL) {
        *normal = uxQueueMessagesWaiting(pq->queues[SYSTEM_EVENT_PRIORITY_NORMAL]);
    }
    
    if (low != NULL) {
        *low = uxQueueMessagesWaiting(pq->queues[SYSTEM_EVENT_PRIORITY_LOW]);
    }
    
    return ESP_OK;
}
