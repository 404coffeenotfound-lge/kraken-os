/**
 * @file main_init_example.c
 * @brief Example of how to initialize system_service from main
 * 
 * This shows the proper way to initialize the system service component
 * from your app_main() function in ESP-IDF.
 */

#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Include system service headers
#include "system_service/system_service.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"

static const char *TAG = "main";

// Store secure key globally (only in main.c!)
static system_secure_key_t g_secure_key = 0;

/**
 * @brief Main application entry point
 * 
 * This is where you initialize the system service.
 * The secure key is generated and returned to you.
 */
void app_main(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Starting application...");
    
    // ========================================================================
    // STEP 1: Initialize system service
    // ========================================================================
    // This MUST be done first, before any other component uses the system
    // The function generates a secure key and returns it to you
    
    ret = system_service_init(&g_secure_key);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize system service: %s", 
                 esp_err_to_name(ret));
        return;  // Critical failure - cannot continue
    }
    
    ESP_LOGI(TAG, "✓ System service initialized");
    ESP_LOGI(TAG, "  Secure key: 0x%08lX (keep this safe!)", g_secure_key);
    
    // ========================================================================
    // STEP 2: Start the system service
    // ========================================================================
    // This starts the event processing task
    // Without this, events won't be dispatched to subscribers
    
    ret = system_service_start(g_secure_key);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start system service: %s", 
                 esp_err_to_name(ret));
        system_service_deinit(g_secure_key);  // Cleanup
        return;
    }
    
    ESP_LOGI(TAG, "✓ System service started");
    ESP_LOGI(TAG, "  Event processing task is now running");
    
    // ========================================================================
    // STEP 3: Initialize your other components
    // ========================================================================
    // Now that system service is running, other components can:
    // - Register themselves as services
    // - Register event types
    // - Subscribe to events
    // - Post events
    
    // Example: Initialize other components
    // wifi_service_init();      // Your WiFi service
    // sensor_service_init();    // Your sensor service
    // display_service_init();   // Your display service
    // etc.
    
    ESP_LOGI(TAG, "✓ Application components initialized");
    
    // ========================================================================
    // STEP 4: Get system statistics (optional)
    // ========================================================================
    // You can query the system at any time using the secure key
    
    vTaskDelay(pdMS_TO_TICKS(1000));  // Wait a bit
    
    uint32_t total_services = 0;
    uint32_t total_events = 0;
    uint32_t total_subscriptions = 0;
    
    ret = system_service_get_stats(g_secure_key, 
                                   &total_services, 
                                   &total_events, 
                                   &total_subscriptions);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "=== System Statistics ===");
        ESP_LOGI(TAG, "  Services registered: %lu", total_services);
        ESP_LOGI(TAG, "  Events processed:    %lu", total_events);
        ESP_LOGI(TAG, "  Active subscriptions: %lu", total_subscriptions);
    }
    
    // ========================================================================
    // STEP 5: Main loop
    // ========================================================================
    // Your main application logic runs here
    // The system service runs in the background
    
    ESP_LOGI(TAG, "Entering main loop...");
    
    while (1) {
        // Your periodic tasks here
        
        vTaskDelay(pdMS_TO_TICKS(10000));  // Sleep 10 seconds
    }
    
    // ========================================================================
    // STEP 6: Cleanup (optional - only if you need to shut down)
    // ========================================================================
    // Normally ESP32 applications run forever, but if you need to cleanup:
    
    // ESP_LOGI(TAG, "Shutting down system service...");
    // system_service_stop(g_secure_key);
    // system_service_deinit(g_secure_key);
    // ESP_LOGI(TAG, "✓ System service stopped");
}
