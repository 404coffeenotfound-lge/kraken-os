#include "input_service.h"
#include "system_service/system_service.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "input_service";

static system_service_id_t input_service_id = 0;
static system_event_type_t input_events[5];
static bool initialized = false;

esp_err_t input_service_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Input service already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing input service...");

    // Register with system service
    ret = system_service_register("input_service", NULL, &input_service_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register with system service: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "✓ Registered with system service (ID: %d)", input_service_id);

    const char *event_names[] = {
        "input.key_left_pressed",
        "input.key_right_pressed",
        "input.key_up_pressed",
        "input.key_down_pressed",
        "input.key_select_pressed"
    };

    for (int i = 0; i < 5; i++) {
        ret = system_event_register_type(event_names[i], &input_events[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register event type '%s'", event_names[i]);
            return ret;
        }
    }

    ESP_LOGI(TAG, "✓ Registered %d event types", 5);

    // Set service state
    system_service_set_state(input_service_id, SYSTEM_SERVICE_STATE_REGISTERED);

    initialized = true;

    ESP_LOGI(TAG, "✓ Input service initialized successfully");
    ESP_LOGI(TAG, "  → Posted INPUT_EVENT_REGISTERED");

    return ESP_OK;
}

esp_err_t input_service_deinit(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Deinitializing input service...");

    system_service_unregister(input_service_id);
    initialized = false;

    ESP_LOGI(TAG, "✓ Input service deinitialized");

    return ESP_OK;
}

esp_err_t input_service_start(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting input service...");

    system_service_set_state(input_service_id, SYSTEM_SERVICE_STATE_RUNNING);

    ESP_LOGI(TAG, "✓ Input service started");

    return ESP_OK;
}

esp_err_t input_service_stop(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Stopping input service...");

    system_service_set_state(input_service_id, SYSTEM_SERVICE_STATE_STOPPING);

    ESP_LOGI(TAG, "✓ Input service stopped");

    return ESP_OK;
}