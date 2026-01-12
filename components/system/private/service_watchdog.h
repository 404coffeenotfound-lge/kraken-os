/**
 * @file service_watchdog.h
 * @brief Service watchdog monitoring and automatic recovery
 * 
 * Monitors service heartbeats and automatically restarts failed services.
 * Critical services trigger safe mode instead of restart.
 */

#ifndef SERVICE_WATCHDOG_H
#define SERVICE_WATCHDOG_H

#include "esp_err.h"
#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Watchdog Initialization
 * ============================================================================ */

/**
 * @brief Initialize watchdog system
 * 
 * Creates watchdog monitoring task and initializes internal structures.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t watchdog_init(void);

/**
 * @brief Start watchdog monitoring
 * 
 * Begins monitoring service heartbeats.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t watchdog_start(void);

/**
 * @brief Stop watchdog monitoring
 * 
 * Stops monitoring but keeps watchdog initialized.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t watchdog_stop(void);

/**
 * @brief Deinitialize watchdog system
 * 
 * Stops monitoring and frees resources.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t watchdog_deinit(void);

/* ============================================================================
 * Service Registration
 * ============================================================================ */

/**
 * @brief Register service with watchdog
 * 
 * @param service_id Service identifier
 * @param config Watchdog configuration (NULL for defaults)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t watchdog_register_service(system_service_id_t service_id, 
                                    const service_watchdog_config_t *config);

/**
 * @brief Unregister service from watchdog
 * 
 * @param service_id Service identifier
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t watchdog_unregister_service(system_service_id_t service_id);

/**
 * @brief Update service heartbeat timestamp
 * 
 * Called by service_manager when service sends heartbeat.
 * 
 * @param service_id Service identifier
 * @param timestamp Current timestamp (ms)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t watchdog_update_heartbeat(system_service_id_t service_id, uint32_t timestamp);

/* ============================================================================
 * Service Control
 * ============================================================================ */

/**
 * @brief Enable watchdog for a service
 * 
 * @param service_id Service identifier
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t watchdog_enable_service(system_service_id_t service_id);

/**
 * @brief Disable watchdog for a service
 * 
 * @param service_id Service identifier
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t watchdog_disable_service(system_service_id_t service_id);

/**
 * @brief Reset restart counter for a service
 * 
 * @param service_id Service identifier
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t watchdog_reset_restart_count(system_service_id_t service_id);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Watchdog statistics
 */
typedef struct {
    uint32_t total_timeouts;        /**< Total watchdog timeouts */
    uint32_t total_restarts;        /**< Total service restarts */
    uint32_t failed_restarts;       /**< Failed restart attempts */
    uint32_t critical_failures;     /**< Critical service failures */
    bool safe_mode_active;          /**< System in safe mode */
} watchdog_stats_t;

/**
 * @brief Get watchdog statistics
 * 
 * @param stats Output statistics structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t watchdog_get_stats(watchdog_stats_t *stats);

/**
 * @brief Log watchdog status
 * 
 * @param tag Log tag to use
 */
void watchdog_log_status(const char *tag);

#ifdef __cplusplus
}
#endif

#endif // SERVICE_WATCHDOG_H
