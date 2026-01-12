/**
 * @file memory_pool.c
 * @brief Memory pool implementation for event data allocation
 * 
 * Uses pre-allocated memory pools to reduce heap fragmentation and improve
 * allocation performance. Falls back to heap for oversized allocations.
 */

#include "memory_pool.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "memory_pool";

/* ============================================================================
 * Pool Block Structure
 * ============================================================================ */

/** Magic number for block validation */
#define POOL_BLOCK_MAGIC 0xDEADBEEF

/** Block header for tracking */
typedef struct pool_block {
    uint32_t magic;                 /**< Magic number for validation */
    memory_pool_size_t pool_id;     /**< Which pool this belongs to */
    struct pool_block *next;        /**< Next free block in pool */
} pool_block_t;

/* ============================================================================
 * Pool Structure
 * ============================================================================ */

typedef struct {
    size_t block_size;              /**< Size of each block (including header) */
    size_t data_size;               /**< Usable data size (block_size - header) */
    uint32_t total_blocks;          /**< Total blocks in pool */
    pool_block_t *free_list;        /**< Free list head */
    void *pool_memory;              /**< Pointer to pool memory */
    SemaphoreHandle_t mutex;        /**< Mutex for thread safety */
    memory_pool_stats_t stats;      /**< Pool statistics */
} memory_pool_t;

/* ============================================================================
 * Global State
 * ============================================================================ */

/** Pool definitions */
static memory_pool_t g_pools[MEMORY_POOL_SIZE_COUNT];

/** Pool initialized flag */
static bool g_pools_initialized = false;

/** Pool size configurations */
static const struct {
    size_t data_size;
    uint32_t count;
} pool_configs[MEMORY_POOL_SIZE_COUNT] = {
    { 64,  CONFIG_SYSTEM_SERVICE_POOL_SIZE_64  },
    { 128, CONFIG_SYSTEM_SERVICE_POOL_SIZE_128 },
    { 256, CONFIG_SYSTEM_SERVICE_POOL_SIZE_256 },
    { 512, CONFIG_SYSTEM_SERVICE_POOL_SIZE_512 },
};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Initialize a single pool
 */
static esp_err_t pool_init_single(memory_pool_size_t pool_id)
{
    memory_pool_t *pool = &g_pools[pool_id];
    
    // Calculate block size (data + header)
    pool->data_size = pool_configs[pool_id].data_size;
    pool->block_size = pool->data_size + sizeof(pool_block_t);
    pool->total_blocks = pool_configs[pool_id].count;
    
    // Skip if pool size is 0
    if (pool->total_blocks == 0) {
        ESP_LOGW(TAG, "Pool %d disabled (size=0)", pool_id);
        pool->pool_memory = NULL;
        pool->free_list = NULL;
        pool->mutex = NULL;
        return ESP_OK;
    }
    
    // Allocate pool memory
    size_t total_size = pool->block_size * pool->total_blocks;
    pool->pool_memory = heap_caps_malloc(total_size, MALLOC_CAP_8BIT);
    if (pool->pool_memory == NULL) {
        ESP_LOGE(TAG, "Failed to allocate pool %d (%zu bytes)", pool_id, total_size);
        return ESP_ERR_NO_MEM;
    }
    
    // Create mutex
    pool->mutex = xSemaphoreCreateMutex();
    if (pool->mutex == NULL) {
        free(pool->pool_memory);
        pool->pool_memory = NULL;
        ESP_LOGE(TAG, "Failed to create mutex for pool %d", pool_id);
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize free list
    pool->free_list = NULL;
    uint8_t *ptr = (uint8_t *)pool->pool_memory;
    for (uint32_t i = 0; i < pool->total_blocks; i++) {
        pool_block_t *block = (pool_block_t *)ptr;
        block->magic = POOL_BLOCK_MAGIC;
        block->pool_id = pool_id;
        block->next = pool->free_list;
        pool->free_list = block;
        ptr += pool->block_size;
    }
    
    // Initialize statistics
    memset(&pool->stats, 0, sizeof(memory_pool_stats_t));
    pool->stats.pool_size = pool->total_blocks;
    pool->stats.blocks_free = pool->total_blocks;
    
    ESP_LOGI(TAG, "Pool %d initialized: %u blocks × %zu bytes = %zu total",
             pool_id, pool->total_blocks, pool->data_size, total_size);
    
    return ESP_OK;
}

/**
 * @brief Deinitialize a single pool
 */
static void pool_deinit_single(memory_pool_size_t pool_id)
{
    memory_pool_t *pool = &g_pools[pool_id];
    
    if (pool->pool_memory != NULL) {
        free(pool->pool_memory);
        pool->pool_memory = NULL;
    }
    
    if (pool->mutex != NULL) {
        vSemaphoreDelete(pool->mutex);
        pool->mutex = NULL;
    }
    
    pool->free_list = NULL;
}

/**
 * @brief Select appropriate pool for size
 */
static memory_pool_size_t select_pool_for_size(size_t size)
{
    for (int i = 0; i < MEMORY_POOL_SIZE_COUNT; i++) {
        if (size <= pool_configs[i].data_size) {
            return (memory_pool_size_t)i;
        }
    }
    return MEMORY_POOL_SIZE_COUNT; // Too large for any pool
}

/**
 * @brief Check if pointer belongs to a pool
 */
static bool is_pool_pointer(const void *ptr, memory_pool_size_t *out_pool_id)
{
    if (ptr == NULL) {
        return false;
    }
    
    // Get block header
    const pool_block_t *block = (const pool_block_t *)((const uint8_t *)ptr - sizeof(pool_block_t));
    
    // Validate magic number
    if (block->magic != POOL_BLOCK_MAGIC) {
        return false;
    }
    
    // Validate pool ID
    if (block->pool_id >= MEMORY_POOL_SIZE_COUNT) {
        return false;
    }
    
    // Validate pointer is within pool memory range
    memory_pool_t *pool = &g_pools[block->pool_id];
    if (pool->pool_memory == NULL) {
        return false;
    }
    
    const uint8_t *pool_start = (const uint8_t *)pool->pool_memory;
    const uint8_t *pool_end = pool_start + (pool->block_size * pool->total_blocks);
    const uint8_t *block_ptr = (const uint8_t *)block;
    
    if (block_ptr < pool_start || block_ptr >= pool_end) {
        return false;
    }
    
    if (out_pool_id != NULL) {
        *out_pool_id = block->pool_id;
    }
    
    return true;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

esp_err_t memory_pool_init(void)
{
    if (g_pools_initialized) {
        ESP_LOGW(TAG, "Memory pools already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing memory pools...");
    
    // Initialize all pools
    for (int i = 0; i < MEMORY_POOL_SIZE_COUNT; i++) {
        esp_err_t ret = pool_init_single((memory_pool_size_t)i);
        if (ret != ESP_OK && pool_configs[i].count > 0) {
            // Cleanup already initialized pools
            for (int j = 0; j < i; j++) {
                pool_deinit_single((memory_pool_size_t)j);
            }
            return ret;
        }
    }
    
    g_pools_initialized = true;
    ESP_LOGI(TAG, "Memory pools initialized successfully");
    
    return ESP_OK;
}

esp_err_t memory_pool_deinit(void)
{
    if (!g_pools_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing memory pools...");
    
    // Deinitialize all pools
    for (int i = 0; i < MEMORY_POOL_SIZE_COUNT; i++) {
        pool_deinit_single((memory_pool_size_t)i);
    }
    
    g_pools_initialized = false;
    ESP_LOGI(TAG, "Memory pools deinitialized");
    
    return ESP_OK;
}

void* memory_pool_alloc(size_t size)
{
    if (!g_pools_initialized || size == 0) {
        return NULL;
    }
    
    // Select appropriate pool
    memory_pool_size_t pool_id = select_pool_for_size(size);
    
    // If too large for any pool, use heap
    if (pool_id >= MEMORY_POOL_SIZE_COUNT) {
        void *ptr = malloc(size);
        ESP_LOGD(TAG, "Heap alloc %zu bytes (too large for pool): %p", size, ptr);
        return ptr;
    }
    
    memory_pool_t *pool = &g_pools[pool_id];
    
    // If pool is disabled, use heap
    if (pool->pool_memory == NULL) {
        void *ptr = malloc(size);
        ESP_LOGD(TAG, "Heap alloc %zu bytes (pool disabled): %p", size, ptr);
        return ptr;
    }
    
    // Try to allocate from pool
    if (xSemaphoreTake(pool->mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire pool mutex");
        return NULL;
    }
    
    pool_block_t *block = pool->free_list;
    void *ptr = NULL;
    
    if (block != NULL) {
        // Remove from free list
        pool->free_list = block->next;
        
        // Update statistics
        pool->stats.blocks_used++;
        pool->stats.blocks_free--;
        pool->stats.total_allocations++;
        if (pool->stats.blocks_used > pool->stats.high_water_mark) {
            pool->stats.high_water_mark = pool->stats.blocks_used;
        }
        
        // Return pointer to data (after header)
        ptr = (void *)((uint8_t *)block + sizeof(pool_block_t));
        
        ESP_LOGD(TAG, "Pool %d alloc %zu bytes: %p (free=%u)", 
                 pool_id, size, ptr, pool->stats.blocks_free);
    } else {
        // Pool exhausted, update statistics
        pool->stats.allocation_failures++;
        ESP_LOGW(TAG, "Pool %d exhausted, falling back to heap", pool_id);
    }
    
    xSemaphoreGive(pool->mutex);
    
    // Fall back to heap if pool exhausted
    if (ptr == NULL) {
        ptr = malloc(size);
        ESP_LOGD(TAG, "Heap alloc %zu bytes (pool full): %p", size, ptr);
    }
    
    return ptr;
}

void memory_pool_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }
    
    // Check if this is a pool pointer
    memory_pool_size_t pool_id;
    if (!is_pool_pointer(ptr, &pool_id)) {
        // Not from pool, free to heap
        free(ptr);
        ESP_LOGD(TAG, "Heap free: %p", ptr);
        return;
    }
    
    // Return to pool
    memory_pool_t *pool = &g_pools[pool_id];
    pool_block_t *block = (pool_block_t *)((uint8_t *)ptr - sizeof(pool_block_t));
    
    if (xSemaphoreTake(pool->mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire pool mutex for free");
        return;
    }
    
    // Add to free list
    block->next = pool->free_list;
    pool->free_list = block;
    
    // Update statistics
    pool->stats.blocks_used--;
    pool->stats.blocks_free++;
    pool->stats.total_frees++;
    
    ESP_LOGD(TAG, "Pool %d free: %p (free=%u)", pool_id, ptr, pool->stats.blocks_free);
    
    xSemaphoreGive(pool->mutex);
}

esp_err_t memory_pool_get_stats(memory_pool_size_t pool_size, memory_pool_stats_t *stats)
{
    if (pool_size >= MEMORY_POOL_SIZE_COUNT || stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_pools_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    memory_pool_t *pool = &g_pools[pool_size];
    
    if (pool->mutex == NULL) {
        // Pool disabled
        memset(stats, 0, sizeof(memory_pool_stats_t));
        return ESP_OK;
    }
    
    if (xSemaphoreTake(pool->mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memcpy(stats, &pool->stats, sizeof(memory_pool_stats_t));
    
    xSemaphoreGive(pool->mutex);
    
    return ESP_OK;
}

esp_err_t memory_pool_get_all_stats(memory_pool_stats_t stats[MEMORY_POOL_SIZE_COUNT])
{
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < MEMORY_POOL_SIZE_COUNT; i++) {
        esp_err_t ret = memory_pool_get_stats((memory_pool_size_t)i, &stats[i]);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    
    return ESP_OK;
}

void memory_pool_reset_stats(void)
{
    if (!g_pools_initialized) {
        return;
    }
    
    for (int i = 0; i < MEMORY_POOL_SIZE_COUNT; i++) {
        memory_pool_t *pool = &g_pools[i];
        if (pool->mutex != NULL) {
            if (xSemaphoreTake(pool->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                pool->stats.total_allocations = 0;
                pool->stats.total_frees = 0;
                pool->stats.allocation_failures = 0;
                pool->stats.high_water_mark = pool->stats.blocks_used;
                xSemaphoreGive(pool->mutex);
            }
        }
    }
}

void memory_pool_log_stats(const char *tag)
{
    if (!g_pools_initialized) {
        ESP_LOGW(tag, "Memory pools not initialized");
        return;
    }
    
    ESP_LOGI(tag, "Memory Pool Statistics:");
    ESP_LOGI(tag, "  Pool | Size | Total | Used | Free | Allocs | Frees | Failures | HWM");
    ESP_LOGI(tag, "  -----|------|-------|------|------|--------|-------|----------|----");
    
    for (int i = 0; i < MEMORY_POOL_SIZE_COUNT; i++) {
        memory_pool_stats_t stats;
        if (memory_pool_get_stats((memory_pool_size_t)i, &stats) == ESP_OK) {
            ESP_LOGI(tag, "  %4d | %4zu | %5u | %4u | %4u | %6u | %5u | %8u | %3u",
                     i,
                     pool_configs[i].data_size,
                     stats.pool_size,
                     stats.blocks_used,
                     stats.blocks_free,
                     stats.total_allocations,
                     stats.total_frees,
                     stats.allocation_failures,
                     stats.high_water_mark);
        }
    }
}

bool memory_pool_check_health(void)
{
    if (!g_pools_initialized) {
        return false;
    }
    
    bool healthy = true;
    
    for (int i = 0; i < MEMORY_POOL_SIZE_COUNT; i++) {
        memory_pool_stats_t stats;
        if (memory_pool_get_stats((memory_pool_size_t)i, &stats) == ESP_OK) {
            // Check for high utilization (>90%)
            if (stats.pool_size > 0) {
                uint32_t utilization = (stats.blocks_used * 100) / stats.pool_size;
                if (utilization > 90) {
                    ESP_LOGW(TAG, "Pool %d high utilization: %u%%", i, utilization);
                    healthy = false;
                }
            }
            
            // Check for allocation failures
            if (stats.allocation_failures > 0) {
                ESP_LOGW(TAG, "Pool %d has %u allocation failures", 
                         i, stats.allocation_failures);
                healthy = false;
            }
        }
    }
    
    return healthy;
}
