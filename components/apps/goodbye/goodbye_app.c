#include "system_service/app_manager.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "system_service/common_events.h"
#include "system_service/memory_utils.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "goodbye_app";

// App entry point
esp_err_t goodbye_app_entry(app_context_t *ctx)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║       GOODBYE APP STARTED!               ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════╝");
    
    system_service_id_t service_id = ctx->service_id;
    
    // Set state to running
    system_service_set_state(service_id, SYSTEM_SERVICE_STATE_RUNNING);
    memory_log_usage(TAG);
    
    ESP_LOGI(TAG, "App info:");
    ESP_LOGI(TAG, "  Name:    %s", ctx->app_info->manifest.name);
    ESP_LOGI(TAG, "  Version: %s", ctx->app_info->manifest.version);
    ESP_LOGI(TAG, "  Author:  %s", ctx->app_info->manifest.author);
    ESP_LOGI(TAG, "  Service ID: %d", service_id);
    
    // Register custom event type
    system_event_type_t goodbye_event;
    esp_err_t ret = system_event_register_type("app.goodbye.countdown", &goodbye_event);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Registered custom event type: %d", goodbye_event);
    }
    
    // Countdown with events
    for (int i = 5; i > 0; i--) {
        ESP_LOGI(TAG, "Goodbye countdown: %d...", i);
        
        // Post countdown event
        system_event_post(service_id, goodbye_event, &i, sizeof(i), SYSTEM_EVENT_PRIORITY_NORMAL);
        
        // Heartbeat
        system_service_heartbeat(service_id);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "Goodbye app task completing...");
    memory_log_usage(TAG);
    
    return ESP_OK;
}

// App exit point
esp_err_t goodbye_app_exit(app_context_t *ctx)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║       GOODBYE APP EXITING                ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════╝");
    
    return ESP_OK;
}

// App manifest
const app_manifest_t goodbye_app_manifest = {
    .name = "goodbye",
    .version = "1.0.0",
    .author = "Kraken Team",
    .entry = goodbye_app_entry,
    .exit = goodbye_app_exit,
    .user_data = NULL
};
