#include "system_service/memory_utils.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "memory_utils";

esp_err_t memory_get_info(memory_info_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(info, 0, sizeof(memory_info_t));

    // Get internal SRAM info
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_INTERNAL);
    info->total_sram = heap_info.total_free_bytes + heap_info.total_allocated_bytes;
    info->free_sram = heap_info.total_free_bytes;
    info->min_free_sram = heap_info.minimum_free_bytes;

    // Get PSRAM info if available
    if (memory_psram_available()) {
        heap_caps_get_info(&heap_info, MALLOC_CAP_SPIRAM);
        info->total_psram = heap_info.total_free_bytes + heap_info.total_allocated_bytes;
        info->free_psram = heap_info.total_free_bytes;
        info->min_free_psram = heap_info.minimum_free_bytes;
    }

    return ESP_OK;
}

void memory_log_usage(const char *tag)
{
    memory_info_t info;
    if (memory_get_info(&info) == ESP_OK) {
        ESP_LOGI(tag ? tag : TAG, "╔════════════════ Memory Status ════════════════╗");
        ESP_LOGI(tag ? tag : TAG, "║ SRAM  (Internal):                            ║");
        ESP_LOGI(tag ? tag : TAG, "║   Total: %6zu KB  Free: %6zu KB (%2zu%%)   ║",
                 info.total_sram / 1024,
                 info.free_sram / 1024,
                 info.total_sram ? (info.free_sram * 100 / info.total_sram) : 0);
        ESP_LOGI(tag ? tag : TAG, "║   Min Free: %6zu KB                          ║",
                 info.min_free_sram / 1024);
        
        if (info.total_psram > 0) {
            ESP_LOGI(tag ? tag : TAG, "║ PSRAM (External):                            ║");
            ESP_LOGI(tag ? tag : TAG, "║   Total: %6zu KB  Free: %6zu KB (%2zu%%)   ║",
                     info.total_psram / 1024,
                     info.free_psram / 1024,
                     info.total_psram ? (info.free_psram * 100 / info.total_psram) : 0);
            ESP_LOGI(tag ? tag : TAG, "║   Min Free: %6zu KB                          ║",
                     info.min_free_psram / 1024);
        } else {
            ESP_LOGI(tag ? tag : TAG, "║ PSRAM: Not available                         ║");
        }
        ESP_LOGI(tag ? tag : TAG, "╚═══════════════════════════════════════════════╝");
    }
}

bool memory_psram_available(void)
{
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0;
}

size_t memory_get_free_sram(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
}

size_t memory_get_free_psram(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

void* memory_alloc_prefer_psram(size_t size)
{
    void *ptr = NULL;
    
    // Try PSRAM first
    if (memory_psram_available()) {
        ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    
    // Fallback to SRAM if PSRAM allocation failed
    if (ptr == NULL) {
        ESP_LOGW(TAG, "PSRAM allocation failed, using SRAM for %zu bytes", size);
        ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    
    return ptr;
}

void* memory_alloc_psram_only(size_t size)
{
    if (!memory_psram_available()) {
        ESP_LOGE(TAG, "PSRAM not available");
        return NULL;
    }
    
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes in PSRAM", size);
    }
    
    return ptr;
}

void* memory_alloc_sram_only(size_t size)
{
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (ptr == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes in SRAM", size);
    }
    
    return ptr;
}
