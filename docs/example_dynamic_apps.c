/**
 * Example: Loading and Running Dynamic Apps in Kraken OS
 * 
 * This example shows how to use the dynamic app loading system.
 */

#include "system_service/app_manager.h"
#include "system_service/app_loader.h"
#include "esp_log.h"

static const char *TAG = "dynamic_app_example";

void example_load_dynamic_app(void)
{
    ESP_LOGI(TAG, "=== Dynamic App Loading Example ===");
    
    // Method 1: Load from partition
    // Assumes you have an app binary flashed to partition "app_store" at offset 0
    
    app_info_t *app_info = NULL;
    esp_err_t ret = app_manager_load_dynamic_from_partition("app_store", 0, &app_info);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Dynamic app loaded successfully!");
        ESP_LOGI(TAG, "  Name: %s", app_info->manifest.name);
        ESP_LOGI(TAG, "  Size: %lu bytes", app_info->app_size);
        ESP_LOGI(TAG, "  State: %d", app_info->state);
        
        // Start the app
        ret = app_manager_start_app(app_info->manifest.name);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✓ App started!");
        } else {
            ESP_LOGE(TAG, "Failed to start app: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "Failed to load dynamic app: %s", esp_err_to_name(ret));
        ESP_LOGI(TAG, "Make sure you have:");
        ESP_LOGI(TAG, "  1. Built the app with build_pic_app.sh");
        ESP_LOGI(TAG, "  2. Flashed it to the app_store partition");
        ESP_LOGI(TAG, "  3. Added app_store partition to partitions.csv");
    }
    
    // Method 2: Load from binary in memory (if you have it)
    /*
    const uint8_t *app_binary = ...; // Your app ELF binary
    size_t binary_size = ...;
    
    loaded_app_t loaded_app;
    ret = app_loader_load_binary(app_binary, binary_size, &loaded_app);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "App loaded from memory!");
        // Call the entry point directly
        app_entry_fn_t entry = (app_entry_fn_t)loaded_app.entry_point;
        
        // Create app context
        app_context_t ctx;
        // ... initialize ctx with system APIs ...
        
        entry(&ctx);
        
        // Cleanup
        app_loader_unload(&loaded_app);
    }
    */
}

void example_list_all_apps(void)
{
    ESP_LOGI(TAG, "=== Listing All Apps ===");
    
    app_info_t apps[APP_MAX_APPS];
    size_t count;
    
    esp_err_t ret = app_manager_list_apps(apps, APP_MAX_APPS, &count);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Found %zu apps:", count);
        
        for (size_t i = 0; i < count; i++) {
            ESP_LOGI(TAG, "  [%zu] %s v%s by %s", 
                     i,
                     apps[i].manifest.name,
                     apps[i].manifest.version,
                     apps[i].manifest.author);
            ESP_LOGI(TAG, "      Source: %s, State: %d, Dynamic: %s",
                     apps[i].source == APP_SOURCE_INTERNAL ? "Built-in" :
                     apps[i].source == APP_SOURCE_STORAGE ? "Storage" : "Remote",
                     apps[i].state,
                     apps[i].is_dynamic ? "Yes" : "No");
            
            if (apps[i].is_dynamic) {
                ESP_LOGI(TAG, "      Size: %lu bytes", apps[i].app_size);
            }
        }
    }
}

void example_manage_app_lifecycle(const char *app_name)
{
    ESP_LOGI(TAG, "=== Managing App: %s ===", app_name);
    
    // Start app
    ESP_LOGI(TAG, "Starting app...");
    esp_err_t ret = app_manager_start_app(app_name);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ App started");
    }
    
    // Wait a bit
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Pause app
    ESP_LOGI(TAG, "Pausing app...");
    ret = app_manager_pause_app(app_name);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ App paused");
    }
    
    // Resume app
    ESP_LOGI(TAG, "Resuming app...");
    ret = app_manager_resume_app(app_name);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ App resumed");
    }
    
    // Wait a bit more
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Stop app
    ESP_LOGI(TAG, "Stopping app...");
    ret = app_manager_stop_app(app_name);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ App stopped");
    }
}
