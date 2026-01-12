/**
 * @file resource_quota.h
 * @brief Per-service resource quota management
 * 
 * Enforces resource limits to prevent misbehaving services from
 * exhausting system resources.
 */

#ifndef RESOURCE_QUOTA_H
#define RESOURCE_QUOTA_H

#include "esp_err.h"
#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * @brief Initialize quota system
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t quota_init(void);

/**
 * @brief Deinitialize quota system
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t quota_deinit(void);

/* ============================================================================
 * Quota Management
 * ============================================================================ */

/**
 * @brief Set quota for a service
 * 
 * @param service_id Service identifier
 * @param quota Quota configuration (NULL for defaults)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t quota_set(system_service_id_t service_id, const service_quota_t *quota);

/**
 * @brief Get quota for a service
 * 
 * @param service_id Service identifier
 * @param quota Output quota configuration
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t quota_get(system_service_id_t service_id, service_quota_t *quota);

/**
 * @brief Get quota usage for a service
 * 
 * @param service_id Service identifier
 * @param usage Output usage statistics
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t quota_get_usage(system_service_id_t service_id, service_quota_usage_t *usage);

/* ============================================================================
 * Quota Checking
 * ============================================================================ */

/**
 * @brief Check if service can post event
 * 
 * Checks event rate quota.
 * 
 * @param service_id Service identifier
 * @return ESP_OK if allowed, ESP_ERR_QUOTA_EVENTS_EXCEEDED if quota exceeded
 */
esp_err_t quota_check_event_post(system_service_id_t service_id);

/**
 * @brief Record event post
 * 
 * Updates event counter for quota tracking.
 * 
 * @param service_id Service identifier
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t quota_record_event_post(system_service_id_t service_id);

/**
 * @brief Check if service can subscribe
 * 
 * Checks subscription quota.
 * 
 * @param service_id Service identifier
 * @return ESP_OK if allowed, ESP_ERR_QUOTA_SUBSCRIPTIONS_EXCEEDED if quota exceeded
 */
esp_err_t quota_check_subscription(system_service_id_t service_id);

/**
 * @brief Record subscription
 * 
 * Updates subscription counter.
 * 
 * @param service_id Service identifier
 * @param add true to add subscription, false to remove
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t quota_record_subscription(system_service_id_t service_id, bool add);

/**
 * @brief Check event data size
 * 
 * @param service_id Service identifier
 * @param data_size Size of event data
 * @return ESP_OK if allowed, ESP_ERR_QUOTA_DATA_SIZE_EXCEEDED if quota exceeded
 */
esp_err_t quota_check_data_size(system_service_id_t service_id, size_t data_size);

/**
 * @brief Record memory allocation
 * 
 * @param service_id Service identifier
 * @param size Size allocated
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t quota_record_memory_alloc(system_service_id_t service_id, size_t size);

/**
 * @brief Record memory free
 * 
 * @param service_id Service identifier
 * @param size Size freed
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t quota_record_memory_free(system_service_id_t service_id, size_t size);

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * @brief Reset quota counters
 * 
 * Called periodically (e.g., every second) to reset rate-limited counters.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t quota_reset_counters(void);

/**
 * @brief Log quota status
 * 
 * @param tag Log tag to use
 */
void quota_log_status(const char *tag);

#ifdef __cplusplus
}
#endif

#endif // RESOURCE_QUOTA_H
