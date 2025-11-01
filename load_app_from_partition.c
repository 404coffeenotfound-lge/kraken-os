// Helper to load app directly from flash partition
#include "esp_partition.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "app_partition_loader";

esp_err_t load_app_from_partition(const char *partition_label, size_t offset, void **out_data, size_t *out_size)
{
    // Find the partition
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_FAT,
        partition_label
    );
    
    if (partition == NULL) {
        ESP_LOGE(TAG, "Partition '%s' not found", partition_label);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Found partition '%s' at 0x%lx, size=%lu", 
             partition_label, partition->address, partition->size);
    
    // Read app header first to get size
    uint8_t header_buf[128];
    esp_err_t ret = esp_partition_read(partition, offset, header_buf, sizeof(header_buf));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read header: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Check magic (0x4150504B = "APPK")
    uint32_t magic = *(uint32_t*)&header_buf[0];
    if (magic != 0x4150504B) {
        ESP_LOGE(TAG, "Invalid magic: 0x%08lX", magic);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get app size from header
    uint32_t app_size = *(uint32_t*)&header_buf[84];
    size_t total_size = 128 + app_size;
    
    ESP_LOGI(TAG, "App size: %lu bytes (total with header: %zu)", app_size, total_size);
    
    // Allocate memory for entire app
    void *app_data = malloc(total_size);
    if (app_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes", total_size);
        return ESP_ERR_NO_MEM;
    }
    
    // Read the entire app
    ret = esp_partition_read(partition, offset, app_data, total_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read app: %s", esp_err_to_name(ret));
        free(app_data);
        return ret;
    }
    
    *out_data = app_data;
    *out_size = total_size;
    
    ESP_LOGI(TAG, "✓ Loaded %zu bytes from partition", total_size);
    
    return ESP_OK;
}
