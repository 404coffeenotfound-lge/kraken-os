/**
 * @file memory_pool.h
 * @brief Memory pool management for event data allocation
 * 
 * Provides pre-allocated memory pools to reduce heap fragmentation
 * and improve allocation performance for event data.
 */

#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Pool Sizes
 * ============================================================================ */

/** Pool block sizes */
typedef enum {
    MEMORY_POOL_SIZE_64 = 0,    /**< 64-byte blocks */
    MEMORY_POOL_SIZE_128,       /**< 128-byte blocks */
    MEMORY_POOL_SIZE_256,       /**< 256-byte blocks */
    MEMORY_POOL_SIZE_512,       /**< 512-byte blocks */
    MEMORY_POOL_SIZE_COUNT      /**< Number of pool sizes */
} memory_pool_size_t;

/* ============================================================================
 * Initialization & Cleanup
 * ============================================================================ */

/**
 * @brief Initialize memory pool system
 * 
 * Creates pre-allocated memory pools for common event data sizes.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t memory_pool_init(void);

/**
 * @brief Deinitialize memory pool system
 * 
 * Frees all memory pools. Any outstanding allocations will be leaked.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t memory_pool_deinit(void);

/* ============================================================================
 * Allocation & Deallocation
 * ============================================================================ */

/**
 * @brief Allocate memory from pool
 * 
 * Attempts to allocate from the appropriate pool based on size.
 * Falls back to heap allocation if pool is exhausted or size is too large.
 * 
 * @param size Size in bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 * 
 * @note Caller must call memory_pool_free() to return memory to pool
 */
void* memory_pool_alloc(size_t size);

/**
 * @brief Free memory back to pool
 * 
 * Returns memory to the appropriate pool, or frees to heap if not from pool.
 * 
 * @param ptr Pointer to memory to free (can be NULL)
 * 
 * @note Safe to call with NULL pointer
 * @note Safe to call with heap-allocated memory
 */
void memory_pool_free(void *ptr);

/* ============================================================================
 * Statistics & Monitoring
 * ============================================================================ */

/**
 * @brief Get statistics for a specific pool
 * 
 * @param pool_size Pool size to query
 * @param stats Output statistics structure
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if pool_size invalid
 */
esp_err_t memory_pool_get_stats(memory_pool_size_t pool_size, memory_pool_stats_t *stats);

/**
 * @brief Get statistics for all pools
 * 
 * @param stats Array of statistics structures (must have MEMORY_POOL_SIZE_COUNT elements)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t memory_pool_get_all_stats(memory_pool_stats_t stats[MEMORY_POOL_SIZE_COUNT]);

/**
 * @brief Reset pool statistics
 * 
 * Resets allocation counters but does not affect actual allocations.
 */
void memory_pool_reset_stats(void);

/**
 * @brief Log pool statistics
 * 
 * Logs detailed statistics for all pools.
 * 
 * @param tag Log tag to use
 */
void memory_pool_log_stats(const char *tag);

/**
 * @brief Check pool health
 * 
 * Checks for potential issues like high utilization or fragmentation.
 * 
 * @return true if pools are healthy, false if issues detected
 */
bool memory_pool_check_health(void);

#ifdef __cplusplus
}
#endif

#endif // MEMORY_POOL_H
