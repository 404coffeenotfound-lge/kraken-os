/**
 * @file heap_monitor.c
 * @brief Heap fragmentation monitoring implementation
 */

#include "heap_monitor.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "heap_monitor";

#define FRAGMENTATION_WARNING_THRESHOLD 30  // 30% fragmentation

esp_err_t heap_monitor_get_stats(heap_stats_t *stats)
{
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);
    
    stats->total_free_bytes = info.total_free_bytes;
    stats->total_allocated_bytes = info.total_allocated_bytes;
    stats->largest_free_block = info.largest_free_block;
    stats->minimum_free_ever = info.minimum_free_bytes;
    
    // Calculate fragmentation
    if (stats->total_free_bytes > 0) {
        stats->fragmentation_percent = 100 - 
            (100 * stats->largest_free_block / stats->total_free_bytes);
    } else {
        stats->fragmentation_percent = 100;
    }
    
    stats->fragmentation_warning = 
        (stats->fragmentation_percent >= FRAGMENTATION_WARNING_THRESHOLD);
    
    return ESP_OK;
}

bool heap_monitor_check_health(void)
{
    heap_stats_t stats;
    if (heap_monitor_get_stats(&stats) != ESP_OK) {
        return false;
    }
    
    // Check for high fragmentation
    if (stats.fragmentation_warning) {
        ESP_LOGW(TAG, "High heap fragmentation: %lu%%", stats.fragmentation_percent);
        return false;
    }
    
    // Check for low free memory (< 10KB)
    if (stats.total_free_bytes < 10240) {
        ESP_LOGW(TAG, "Low free heap: %zu bytes", stats.total_free_bytes);
        return false;
    }
    
    return true;
}

void heap_monitor_log_stats(const char *tag)
{
    heap_stats_t stats;
    if (heap_monitor_get_stats(&stats) != ESP_OK) {
        ESP_LOGW(tag, "Failed to get heap stats");
        return;
    }
    
    ESP_LOGI(tag, "Heap Statistics:");
    ESP_LOGI(tag, "  Total Free: %zu bytes", stats.total_free_bytes);
    ESP_LOGI(tag, "  Total Allocated: %zu bytes", stats.total_allocated_bytes);
    ESP_LOGI(tag, "  Largest Free Block: %zu bytes", stats.largest_free_block);
    ESP_LOGI(tag, "  Minimum Free Ever: %zu bytes", stats.minimum_free_ever);
    ESP_LOGI(tag, "  Fragmentation: %lu%% %s",
             stats.fragmentation_percent,
             stats.fragmentation_warning ? "[WARNING]" : "[OK]");
}

uint32_t heap_monitor_get_fragmentation(void)
{
    heap_stats_t stats;
    if (heap_monitor_get_stats(&stats) != ESP_OK) {
        return 100;
    }
    return stats.fragmentation_percent;
}
