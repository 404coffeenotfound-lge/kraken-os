/**
 * @file priority_queue.h
 * @brief Priority-based event queue implementation
 * 
 * Provides separate queues for different priority levels to ensure
 * high-priority events are processed before low-priority ones.
 */

#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "esp_err.h"
#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Priority Queue Handle
 * ============================================================================ */

typedef struct priority_queue* priority_queue_handle_t;

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * @brief Create priority queue system
 * 
 * Creates separate queues for each priority level.
 * 
 * @param handle Output queue handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t priority_queue_create(priority_queue_handle_t *handle);

/**
 * @brief Destroy priority queue system
 * 
 * @param handle Queue handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t priority_queue_destroy(priority_queue_handle_t handle);

/* ============================================================================
 * Queue Operations
 * ============================================================================ */

/**
 * @brief Post event to priority queue
 * 
 * @param handle Queue handle
 * @param event Event to post (will be copied)
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if queue full, error code otherwise
 */
esp_err_t priority_queue_post(priority_queue_handle_t handle,
                               const system_event_t *event,
                               uint32_t timeout_ms);

/**
 * @brief Receive event from priority queue
 * 
 * Receives highest priority event available.
 * Priority order: CRITICAL > HIGH > NORMAL > LOW
 * 
 * @param handle Queue handle
 * @param event Output event structure
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if no events, error code otherwise
 */
esp_err_t priority_queue_receive(priority_queue_handle_t handle,
                                  system_event_t *event,
                                  uint32_t timeout_ms);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Get queue statistics
 * 
 * @param handle Queue handle
 * @param stats Output statistics structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t priority_queue_get_stats(priority_queue_handle_t handle,
                                    event_queue_stats_t *stats);

/**
 * @brief Reset queue statistics
 * 
 * @param handle Queue handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t priority_queue_reset_stats(priority_queue_handle_t handle);

/**
 * @brief Get current queue depths
 * 
 * @param handle Queue handle
 * @param high Output high priority depth
 * @param normal Output normal priority depth
 * @param low Output low priority depth
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t priority_queue_get_depths(priority_queue_handle_t handle,
                                     uint32_t *high,
                                     uint32_t *normal,
                                     uint32_t *low);

#ifdef __cplusplus
}
#endif

#endif // PRIORITY_QUEUE_H
