/**
 * @file example_app.c
 * @brief Example dynamic app using the Kraken OS App SDK
 * 
 * This demonstrates how to create a simple dynamic app that:
 * - Uses the convenient app_sdk.h macros
 * - Subscribes to system events
 * - Posts custom events
 * - Manages state and heartbeat
 */

#include "app_sdk.h"

static const char *TAG = "example_app";

// App state
typedef struct {
    int counter;
    system_event_type_t custom_event;
    bool running;
} example_app_state_t;

// Static app data (will be placed in PSRAM)
APP_STATIC_DATA example_app_state_t app_state;

/**
 * @brief Event handler for system events
 */
static void on_system_event(const system_event_t *event, void *user_data)
{
    app_context_t *ctx = (app_context_t *)user_data;
    
    APP_LOGI(TAG, "Received system event type: %d", event->event_type);
    
    // Handle different event types
    if (event->data && event->data_size > 0) {
        APP_LOGI(TAG, "Event data size: %zu bytes", event->data_size);
    }
}

/**
 * @brief Event handler for custom app events
 */
static void on_custom_event(const system_event_t *event, void *user_data)
{
    if (event->data && event->data_size == sizeof(int)) {
        int *value = (int *)event->data;
        APP_LOGI(TAG, "Custom event received with value: %d", *value);
    }
}

/**
 * @brief App entry point
 */
esp_err_t example_app_entry(app_context_t *ctx)
{
    // Print banner
    app_print_banner(TAG, "EXAMPLE APP STARTED");
    
    // Print app info
    app_print_info(ctx, TAG);
    
    // Log memory usage
    memory_log_usage(TAG);
    
    // Initialize app state
    app_state.counter = 0;
    app_state.running = true;
    
    // Set app to running state
    APP_SET_RUNNING(ctx);
    
    // Register custom event type
    esp_err_t ret = APP_REGISTER_EVENT(ctx, "example_app.counter", &app_state.custom_event);
    if (ret != ESP_OK) {
        APP_LOGE(TAG, "Failed to register custom event");
        return ret;
    }
    APP_LOGI(TAG, "✓ Registered custom event type: %d", app_state.custom_event);
    
    // Subscribe to system events
    system_event_type_t system_startup;
    ret = APP_REGISTER_EVENT(ctx, "system.startup", &system_startup);
    if (ret == ESP_OK) {
        APP_SUBSCRIBE(ctx, system_startup, on_system_event, ctx);
        APP_LOGI(TAG, "✓ Subscribed to system.startup events");
    }
    
    // Subscribe to our own custom events (for demonstration)
    APP_SUBSCRIBE(ctx, app_state.custom_event, on_custom_event, ctx);
    APP_LOGI(TAG, "✓ Subscribed to custom events");
    
    // Allocate some dynamic memory (in PSRAM)
    size_t buffer_size = 1024;
    uint8_t *buffer = APP_ALLOC(buffer_size);
    if (buffer == NULL) {
        APP_LOGE(TAG, "Failed to allocate buffer");
        return ESP_ERR_NO_MEM;
    }
    APP_LOGI(TAG, "✓ Allocated %zu bytes buffer at %p", buffer_size, buffer);
    
    // Main app loop
    APP_LOGI(TAG, "");
    APP_LOGI(TAG, "Starting main loop...");
    
    app_timer_t timer;
    app_timer_start(&timer);
    
    while (app_state.running && app_state.counter < 10) {
        app_state.counter++;
        
        APP_LOGI(TAG, "Loop iteration %d/10", app_state.counter);
        
        // Update heartbeat
        APP_HEARTBEAT(ctx);
        
        // Post custom event with counter value
        APP_POST_EVENT(ctx, app_state.custom_event, 
                      &app_state.counter, sizeof(app_state.counter));
        
        // Use the buffer for something
        memset(buffer, app_state.counter, buffer_size);
        
        // Log elapsed time
        if (app_state.counter % 3 == 0) {
            APP_LOGI(TAG, "Elapsed time: %lu seconds", app_timer_elapsed_sec(&timer));
        }
        
        // Sleep for 1 second
        APP_DELAY_SEC(1);
    }
    
    // Cleanup
    APP_LOGI(TAG, "");
    APP_LOGI(TAG, "App loop finished. Cleaning up...");
    
    APP_FREE(buffer);
    APP_LOGI(TAG, "✓ Freed buffer");
    
    // Unsubscribe from events
    APP_UNSUBSCRIBE(ctx, app_state.custom_event);
    APP_LOGI(TAG, "✓ Unsubscribed from events");
    
    // Log final memory usage
    memory_log_usage(TAG);
    
    app_print_banner(TAG, "EXAMPLE APP FINISHED");
    
    return ESP_OK;
}

/**
 * @brief App exit point
 */
esp_err_t example_app_exit(app_context_t *ctx)
{
    APP_LOGI(TAG, "App exit called");
    
    // Signal app to stop (if it's still running)
    app_state.running = false;
    
    // Cleanup any remaining resources
    // (in this simple example, everything is already cleaned up)
    
    return ESP_OK;
}

// Define the app manifest
// This will be visible to the loader
KRAKEN_APP_MANIFEST(
    "example_app",      // App name
    "1.0.0",           // Version
    "Kraken Team",     // Author
    example_app_entry, // Entry function
    example_app_exit   // Exit function
);
