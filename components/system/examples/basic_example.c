#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "system_service/system_service.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"

static const char *TAG = "example";

static system_service_id_t sensor_service_id;
static system_service_id_t display_service_id;
static system_event_type_t sensor_data_event;
static system_event_type_t system_alert_event;

typedef struct {
    float temperature;
    float humidity;
} sensor_data_t;

void display_event_handler(const system_event_t *event, void *user_data)
{
    if (event->event_type == sensor_data_event && event->data != NULL) {
        sensor_data_t *data = (sensor_data_t *)event->data;
        ESP_LOGI(TAG, "[Display Service] Temperature: %.2f°C, Humidity: %.2f%%", 
                 data->temperature, data->humidity);
    } else if (event->event_type == system_alert_event) {
        ESP_LOGW(TAG, "[Display Service] System alert received!");
    }
}

void sensor_event_handler(const system_event_t *event, void *user_data)
{
    if (event->event_type == system_alert_event) {
        ESP_LOGI(TAG, "[Sensor Service] Alert acknowledged");
    }
}

void sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Sensor task started");
    
    for (int i = 0; i < 10; i++) {
        sensor_data_t data = {
            .temperature = 20.0f + (i * 0.5f),
            .humidity = 50.0f + (i * 2.0f)
        };
        
        system_event_post(sensor_service_id,
                         sensor_data_event,
                         &data,
                         sizeof(data),
                         SYSTEM_EVENT_PRIORITY_NORMAL);
        
        system_service_heartbeat(sensor_service_id);
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret;
    system_secure_key_t secure_key;
    
    ESP_LOGI(TAG, "=== System Service Example ===");
    
    // Step 1: Initialize system service (only from main)
    ret = system_service_init(&secure_key);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize system service: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "System service initialized with secure key: 0x%08lX", secure_key);
    
    // Step 2: Start system service
    ret = system_service_start(secure_key);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start system service");
        return;
    }
    ESP_LOGI(TAG, "System service started");
    
    // Step 3: Register event types
    ret = system_event_register_type("sensor_data", &sensor_data_event);
    ESP_LOGI(TAG, "Registered event type 'sensor_data': %d", sensor_data_event);
    
    ret = system_event_register_type("system_alert", &system_alert_event);
    ESP_LOGI(TAG, "Registered event type 'system_alert': %d", system_alert_event);
    
    // Step 4: Register services
    ret = system_service_register("sensor_service", NULL, &sensor_service_id);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Sensor service registered with ID: %d", sensor_service_id);
        system_service_set_state(sensor_service_id, SYSTEM_SERVICE_STATE_RUNNING);
    }
    
    ret = system_service_register("display_service", NULL, &display_service_id);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Display service registered with ID: %d", display_service_id);
        system_service_set_state(display_service_id, SYSTEM_SERVICE_STATE_RUNNING);
    }
    
    // Step 5: Subscribe to events
    system_event_subscribe(display_service_id, sensor_data_event, display_event_handler, NULL);
    system_event_subscribe(display_service_id, system_alert_event, display_event_handler, NULL);
    system_event_subscribe(sensor_service_id, system_alert_event, sensor_event_handler, NULL);
    
    ESP_LOGI(TAG, "Event subscriptions configured");
    
    // Step 6: Start sensor task
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    
    // Step 7: Wait a bit, then send a system alert
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    ESP_LOGI(TAG, "Posting system alert...");
    system_event_post(0, system_alert_event, NULL, 0, SYSTEM_EVENT_PRIORITY_HIGH);
    
    // Step 8: Get statistics
    vTaskDelay(pdMS_TO_TICKS(15000));
    
    uint32_t total_services, total_events, total_subscriptions;
    system_service_get_stats(secure_key, &total_services, &total_events, &total_subscriptions);
    
    ESP_LOGI(TAG, "=== System Statistics ===");
    ESP_LOGI(TAG, "Total Services: %lu", total_services);
    ESP_LOGI(TAG, "Total Events Processed: %lu", total_events);
    ESP_LOGI(TAG, "Total Subscriptions: %lu", total_subscriptions);
    
    // Step 9: List all services
    system_service_info_t services[SYSTEM_SERVICE_MAX_SERVICES];
    uint32_t count;
    system_service_list_all(services, SYSTEM_SERVICE_MAX_SERVICES, &count);
    
    ESP_LOGI(TAG, "=== Registered Services ===");
    for (uint32_t i = 0; i < count; i++) {
        ESP_LOGI(TAG, "Service: %s (ID: %d, State: %d)", 
                 services[i].name, services[i].service_id, services[i].state);
    }
    
    // Keep running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
