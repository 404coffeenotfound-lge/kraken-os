/**
 * @file service_dependencies.h
 * @brief Service dependency management and initialization ordering
 * 
 * Manages dependencies between services to ensure correct initialization order.
 * Detects circular dependencies and provides topological sort.
 */

#ifndef SERVICE_DEPENDENCIES_H
#define SERVICE_DEPENDENCIES_H

#include "esp_err.h"
#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * @brief Initialize dependency system
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t dependencies_init(void);

/**
 * @brief Deinitialize dependency system
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t dependencies_deinit(void);

/* ============================================================================
 * Dependency Management
 * ============================================================================ */

/**
 * @brief Register service dependency
 * 
 * Declares that 'service_name' depends on 'depends_on'.
 * 
 * @param service_name Name of the service
 * @param depends_on Name of the service it depends on
 * @return ESP_OK on success, ESP_ERR_SERVICE_CIRCULAR_DEPENDENCY if circular dependency detected
 */
esp_err_t dependencies_add(const char *service_name, const char *depends_on);

/**
 * @brief Register multiple dependencies
 * 
 * @param dependency Service dependency descriptor
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t dependencies_add_multiple(const service_dependency_t *dependency);

/**
 * @brief Get initialization order
 * 
 * Returns services in dependency order (dependencies first).
 * 
 * @param order Output array of service names
 * @param max_count Maximum number of entries in order array
 * @param out_count Output actual number of services
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t dependencies_get_init_order(const char **order, size_t max_count, size_t *out_count);

/**
 * @brief Check if service can be initialized
 * 
 * Checks if all dependencies are already initialized.
 * 
 * @param service_name Service to check
 * @return ESP_OK if can initialize, ESP_ERR_SERVICE_DEPENDENCY_FAILED if dependencies not met
 */
esp_err_t dependencies_check_ready(const char *service_name);

/**
 * @brief Mark service as initialized
 * 
 * @param service_name Service name
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t dependencies_mark_initialized(const char *service_name);

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * @brief Log dependency graph
 * 
 * @param tag Log tag to use
 */
void dependencies_log_graph(const char *tag);

#ifdef __cplusplus
}
#endif

#endif // SERVICE_DEPENDENCIES_H
