/**
 * @file app_context_refcount.h
 * @brief App context reference counting and lifetime management
 * 
 * Ensures app contexts remain valid while in use.
 */

#ifndef APP_CONTEXT_REFCOUNT_H
#define APP_CONTEXT_REFCOUNT_H

#include "esp_err.h"
#include "system_service/system_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Reference Counting
 * ============================================================================ */

/**
 * @brief Acquire reference to app context
 * 
 * Increments reference count. Context won't be freed while references exist.
 * 
 * @param service_id App's service ID
 * @return ESP_OK on success, ESP_ERR_APP_CONTEXT_INVALID if context invalid
 */
esp_err_t app_context_acquire(system_service_id_t service_id);

/**
 * @brief Release reference to app context
 * 
 * Decrements reference count. Context may be freed when count reaches zero.
 * 
 * @param service_id App's service ID
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_context_release(system_service_id_t service_id);

/**
 * @brief Get current reference count
 * 
 * @param service_id App's service ID
 * @param count Output reference count
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_context_get_refcount(system_service_id_t service_id, uint32_t *count);

/**
 * @brief Check if context is valid
 * 
 * @param service_id App's service ID
 * @return true if valid, false otherwise
 */
bool app_context_is_valid(system_service_id_t service_id);

/**
 * @brief Mark context for deletion
 * 
 * Context will be deleted when reference count reaches zero.
 * 
 * @param service_id App's service ID
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_context_mark_for_deletion(system_service_id_t service_id);

#ifdef __cplusplus
}
#endif

#endif // APP_CONTEXT_REFCOUNT_H
