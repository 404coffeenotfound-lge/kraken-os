/**
 * @file handler_monitor.h
 * @brief Event handler execution monitoring
 * 
 * Monitors event handler execution time and logs warnings for slow handlers.
 */

#ifndef HANDLER_MONITOR_H
#define HANDLER_MONITOR_H

#include "esp_err.h"
#include "system_service/system_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Handler Monitoring
 * ============================================================================ */

/**
 * @brief Monitor handler execution
 * 
 * Wraps handler execution with timing and timeout monitoring.
 * 
 * @param handler Handler function to execute
 * @param event Event to pass to handler
 * @param user_data User data to pass to handler
 * @param service_id Service ID for logging
 * @return ESP_OK if handler completed, ESP_ERR_EVENT_HANDLER_TIMEOUT if timeout
 */
esp_err_t handler_monitor_execute(system_event_handler_t handler,
                                   const system_event_t *event,
                                   void *user_data,
                                   system_service_id_t service_id);

/**
 * @brief Get handler execution statistics
 * 
 * @param service_id Service identifier
 * @param avg_time_us Output average execution time in microseconds
 * @param max_time_us Output maximum execution time in microseconds
 * @param timeout_count Output number of timeouts
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t handler_monitor_get_stats(system_service_id_t service_id,
                                     uint32_t *avg_time_us,
                                     uint32_t *max_time_us,
                                     uint32_t *timeout_count);

#ifdef __cplusplus
}
#endif

#endif // HANDLER_MONITOR_H
