#include "system_service/app_manager.h"
#include "system_service/memory_utils.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "goodbye_app";

// Example: Large app buffer in PSRAM
APP_DATA_ATTR static uint8_t countdown_data[8192];

// Event handler for network events
static void network_event_handler(const system_event_t *event, void *user_data)
{
    ESP_LOGI(TAG, "Received network event type: %d", event->event_type);
}

esp_err_t goodbye_app_entry(app_context_t *ctx)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║       GOODBYE APP STARTED!               ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════╝");
    
    // Log memory usage
    memory_log_usage(TAG);
    
    ESP_LOGI(TAG, "App info:");
    ESP_LOGI(TAG, "  Name:    %s", ctx->app_info->manifest.name);
    ESP_LOGI(TAG, "  Version: %s", ctx->app_info->manifest.version);
    ESP_LOGI(TAG, "  Author:  %s", ctx->app_info->manifest.author);
    ESP_LOGI(TAG, "  Service ID: %d (registered with system_service)", ctx->service_id);
    ESP_LOGI(TAG, "  Static buffer: %zu bytes in PSRAM", sizeof(countdown_data));
    
    // Allocate dynamic buffer in PSRAM
    void *temp_buffer = APP_MALLOC(20000);
    if (temp_buffer) {
        ESP_LOGI(TAG, "✓ Allocated 20KB temp buffer in PSRAM");
    }
    
    // Register custom event
    system_event_type_t goodbye_event;
    esp_err_t ret = ctx->register_event_type("app.goodbye.countdown", &goodbye_event);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Registered custom event type: %d", goodbye_event);
    }
    
    // Subscribe to network events (apps can interact with services!)
    system_event_type_t network_connected;
    ret = ctx->register_event_type("network.connected", &network_connected);
    if (ret == ESP_OK) {
        ctx->subscribe_event(ctx->service_id, network_connected,
                           network_event_handler, NULL);
        ESP_LOGI(TAG, "✓ Subscribed to network.connected events");
    }
    
    // Update state
    ctx->set_state(ctx->service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    // Countdown with event posting
    for (int i = 5; i > 0; i--) {
        ESP_LOGI(TAG, "Goodbye countdown: %d...", i);
        
        // Post countdown event
        ctx->post_event(ctx->service_id, goodbye_event,
                       &i, sizeof(i),
                       SYSTEM_EVENT_PRIORITY_NORMAL);
        
        // Heartbeat
        ctx->heartbeat(ctx->service_id);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "Goodbye app task completing...");
    
    // Cleanup
    if (temp_buffer) {
        free(temp_buffer);
        ESP_LOGI(TAG, "✓ Freed temp buffer");
    }
    
    memory_log_usage(TAG);
    
    return ESP_OK;
}

esp_err_t goodbye_app_exit(app_context_t *ctx)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║       GOODBYE APP EXITING                ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════╝");
    
    // Cleanup
    system_event_type_t network_connected;
    if (ctx->register_event_type("network.connected", &network_connected) == ESP_OK) {
        ctx->unsubscribe_event(ctx->service_id, network_connected);
    }
    
    return ESP_OK;
}

// App manifest (to be registered with app_manager)
const app_manifest_t goodbye_app_manifest = {
    .name = "goodbye",
    .version = "1.0.0",
    .author = "Kraken Team",
    .entry = goodbye_app_entry,
    .exit = goodbye_app_exit,
    .user_data = NULL
};
