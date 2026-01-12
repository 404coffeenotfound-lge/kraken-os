/**
 * @file log_control.h
 * @brief Per-service log level control
 * 
 * Allows runtime control of log levels for individual services.
 */

#ifndef LOG_CONTROL_H
#define LOG_CONTROL_H

#include "esp_err.h"
#include "esp_log.h"
#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Log Level Control
 * ============================================================================ */

/**
 * @brief Set log level for a service
 * 
 * @param service_id Service identifier
 * @param level Log level (ESP_LOG_NONE to ESP_LOG_VERBOSE)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t log_control_set_level(system_service_id_t service_id, esp_log_level_t level);

/**
 * @brief Get log level for a service
 * 
 * @param service_id Service identifier
 * @param level Output log level
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t log_control_get_level(system_service_id_t service_id, esp_log_level_t *level);

/**
 * @brief Set log level by service name
 * 
 * @param service_name Service name
 * @param level Log level
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t log_control_set_level_by_name(const char *service_name, esp_log_level_t level);

/**
 * @brief Reset all service log levels to default
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t log_control_reset_all(void);

/**
 * @brief Log current log level configuration
 * 
 * @param tag Log tag to use
 */
void log_control_log_status(const char *tag);

#ifdef __cplusplus
}
#endif

#endif // LOG_CONTROL_H
