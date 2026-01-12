/**
 * @file heap_monitor.h
 * @brief Heap fragmentation monitoring
 * 
 * Monitors heap health and fragmentation levels.
 */

#ifndef HEAP_MONITOR_H
#define HEAP_MONITOR_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Heap Statistics
 * ============================================================================ */

typedef struct {
    size_t total_free_bytes;        /**< Total free heap */
    size_t total_allocated_bytes;   /**< Total allocated heap */
    size_t largest_free_block;      /**< Largest contiguous free block */
    size_t minimum_free_ever;       /**< Minimum free heap ever */
    uint32_t fragmentation_percent; /**< Fragmentation percentage (0-100) */
    bool fragmentation_warning;     /**< Fragmentation above threshold */
} heap_stats_t;

/* ============================================================================
 * Monitoring Functions
 * ============================================================================ */

/**
 * @brief Get current heap statistics
 * 
 * @param stats Output statistics structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t heap_monitor_get_stats(heap_stats_t *stats);

/**
 * @brief Check heap health
 * 
 * Checks for high fragmentation or low free memory.
 * 
 * @return true if heap is healthy, false if issues detected
 */
bool heap_monitor_check_health(void);

/**
 * @brief Log heap statistics
 * 
 * @param tag Log tag to use
 */
void heap_monitor_log_stats(const char *tag);

/**
 * @brief Get fragmentation percentage
 * 
 * Fragmentation = 100 * (1 - largest_block / total_free)
 * 
 * @return Fragmentation percentage (0-100)
 */
uint32_t heap_monitor_get_fragmentation(void);

#ifdef __cplusplus
}
#endif

#endif // HEAP_MONITOR_H
