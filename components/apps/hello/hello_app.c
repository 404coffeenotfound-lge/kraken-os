#include "system_service/app_manager.h"
#include "system_service/memory_utils.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hello_app";

// Example: Large app buffer in PSRAM (static allocation)
APP_DATA_ATTR static uint8_t app_large_buffer[10240];

// Event handler for app events
static void hello_event_handler(const system_event_t *event, void *user_data)
{
    ESP_LOGI(TAG, "Received event type: %d", event->event_type);
}

esp_err_t hello_app_entry(app_context_t *ctx)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║         HELLO APP STARTED!               ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════╝");
    
    // Log memory before app allocations
    memory_log_usage(TAG);
    
    ESP_LOGI(TAG, "App info:");
    ESP_LOGI(TAG, "  Name:    %s", ctx->app_info->manifest.name);
    ESP_LOGI(TAG, "  Version: %s", ctx->app_info->manifest.version);
    ESP_LOGI(TAG, "  Author:  %s", ctx->app_info->manifest.author);
    ESP_LOGI(TAG, "  Service ID: %d (registered with system_service)", ctx->service_id);
    
    // Example: Allocate app data in PSRAM
    size_t buffer_size = 50000; // 50KB buffer for app data
    uint8_t *app_data = APP_MALLOC(buffer_size);
    if (app_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate app data buffer in PSRAM");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "✓ Allocated %zu bytes in PSRAM for app data", buffer_size);
    ESP_LOGI(TAG, "✓ Static buffer: %zu bytes in PSRAM", sizeof(app_large_buffer));
    
    // Register custom event types (full event_bus access)
    system_event_type_t hello_event_type;
    esp_err_t ret = ctx->register_event_type("app.hello.custom", &hello_event_type);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Registered custom event type: %d", hello_event_type);
    }
    
    // Subscribe to audio service events (apps can interact with services!)
    system_event_type_t audio_started_event;
    ret = ctx->register_event_type("audio.started", &audio_started_event);
    if (ret == ESP_OK) {
        ctx->subscribe_event(ctx->service_id, audio_started_event, 
                           hello_event_handler, NULL);
        ESP_LOGI(TAG, "✓ Subscribed to audio.started events");
    }
    
    // Update state (full service management access)
    ctx->set_state(ctx->service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    // Simulate some work and post events
    for (int i = 0; i < 5; i++) {
        ESP_LOGI(TAG, "Hello iteration %d/5", i + 1);
        
        // Post custom event
        uint32_t iteration = i + 1;
        ctx->post_event(ctx->service_id, hello_event_type,
                       &iteration, sizeof(iteration),
                       SYSTEM_EVENT_PRIORITY_NORMAL);
        
        // Heartbeat to show app is alive
        ctx->heartbeat(ctx->service_id);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "Hello app task completing...");
    
    // Cleanup: Free PSRAM allocation
    free(app_data);
    ESP_LOGI(TAG, "✓ Freed app data buffer");
    
    // Log memory after cleanup
    memory_log_usage(TAG);
    
    return ESP_OK;
}

esp_err_t hello_app_exit(app_context_t *ctx)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║         HELLO APP EXITING                ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════╝");
    
    // Cleanup - unsubscribe from events
    system_event_type_t audio_started_event;
    if (ctx->register_event_type("audio.started", &audio_started_event) == ESP_OK) {
        ctx->unsubscribe_event(ctx->service_id, audio_started_event);
    }
    
    return ESP_OK;
}

// App manifest (to be registered with app_manager)
const app_manifest_t hello_app_manifest = {
    .name = "hello",
    .version = "1.0.0",
    .author = "Kraken Team",
    .entry = hello_app_entry,
    .exit = hello_app_exit,
    .user_data = NULL
};
