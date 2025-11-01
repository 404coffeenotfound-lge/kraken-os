#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include "esp_err.h"
#include "esp_heap_caps.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Memory allocation strategies for ESP32-S3
 * 
 * SRAM (Internal, fast): ~520KB - Use for system services, ISRs, critical data
 * PSRAM (External, slower): Up to 32MB - Use for apps, large buffers, non-critical data
 */

// Memory allocation macros for apps
#define APP_MALLOC(size)            heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define APP_CALLOC(n, size)         heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define APP_REALLOC(ptr, size)      heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

// Memory allocation macros for system services (internal SRAM)
#define SYSTEM_MALLOC(size)         heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#define SYSTEM_CALLOC(n, size)      heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#define SYSTEM_REALLOC(ptr, size)   heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)

// DMA-capable allocations (must be in internal SRAM for DMA)
#define DMA_MALLOC(size)            heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT)
#define DMA_CALLOC(n, size)         heap_caps_calloc(n, size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT)

// Static allocation attributes for large app buffers
#define APP_DATA_ATTR               EXT_RAM_BSS_ATTR

typedef struct {
    size_t total_sram;
    size_t free_sram;
    size_t min_free_sram;
    size_t total_psram;
    size_t free_psram;
    size_t min_free_psram;
} memory_info_t;

/**
 * @brief Get current memory information
 */
esp_err_t memory_get_info(memory_info_t *info);

/**
 * @brief Log current memory usage
 */
void memory_log_usage(const char *tag);

/**
 * @brief Check if PSRAM is available
 */
bool memory_psram_available(void);

/**
 * @brief Get free SRAM (internal memory)
 */
size_t memory_get_free_sram(void);

/**
 * @brief Get free PSRAM (external memory)
 */
size_t memory_get_free_psram(void);

/**
 * @brief Allocate memory preferring PSRAM, fallback to SRAM
 */
void* memory_alloc_prefer_psram(size_t size);

/**
 * @brief Allocate memory strictly in PSRAM (fail if not available)
 */
void* memory_alloc_psram_only(size_t size);

/**
 * @brief Allocate memory strictly in SRAM (fail if not available)
 */
void* memory_alloc_sram_only(size_t size);

#ifdef __cplusplus
}
#endif

#endif // MEMORY_UTILS_H
