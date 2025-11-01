#include "bluetooth_service.h"
#include "system_service/system_service.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "bluetooth_service";

static system_service_id_t bt_service_id = 0;
static system_event_type_t bt_events[8];
static bool initialized = false;
static bool is_connected = false;

esp_err_t bluetooth_service_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Bluetooth service already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing bluetooth service...");
    
    // Register with system service
    ret = system_service_register("bluetooth_service", NULL, &bt_service_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register with system service: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ Registered with system service (ID: %d)", bt_service_id);
    
    // Register event types
    const char *event_names[] = {
        "bluetooth.registered",
        "bluetooth.started",
        "bluetooth.stopped",
        "bluetooth.connected",
        "bluetooth.disconnected",
        "bluetooth.pairing_request",
        "bluetooth.data_received",
        "bluetooth.error"
    };
    
    for (int i = 0; i < 8; i++) {
        ret = system_event_register_type(event_names[i], &bt_events[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register event type '%s'", event_names[i]);
            return ret;
        }
    }
    
    ESP_LOGI(TAG, "✓ Registered %d event types", 8);
    
    // Set service state
    system_service_set_state(bt_service_id, SYSTEM_SERVICE_STATE_REGISTERED);
    
    initialized = true;
    
    // Post registration event
    system_event_post(bt_service_id,
                     bt_events[BT_EVENT_REGISTERED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Bluetooth service initialized successfully");
    ESP_LOGI(TAG, "  → Posted BT_EVENT_REGISTERED");
    
    return ESP_OK;
}

esp_err_t bluetooth_service_deinit(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing bluetooth service...");
    
    system_service_unregister(bt_service_id);
    initialized = false;
    
    ESP_LOGI(TAG, "✓ Bluetooth service deinitialized");
    
    return ESP_OK;
}

esp_err_t bluetooth_service_start(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting bluetooth service...");
    
    system_service_set_state(bt_service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    // Post started event
    system_event_post(bt_service_id,
                     bt_events[BT_EVENT_STARTED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Bluetooth service started");
    ESP_LOGI(TAG, "  → Posted BT_EVENT_STARTED");
    
    return ESP_OK;
}

esp_err_t bluetooth_service_stop(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Stopping bluetooth service...");
    
    system_service_set_state(bt_service_id, SYSTEM_SERVICE_STATE_STOPPING);
    
    // Post stopped event
    system_event_post(bt_service_id,
                     bt_events[BT_EVENT_STOPPED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Bluetooth service stopped");
    
    return ESP_OK;
}

esp_err_t bluetooth_scan_start(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting BT scan...");
    system_service_heartbeat(bt_service_id);
    
    return ESP_OK;
}

esp_err_t bluetooth_scan_stop(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Stopping BT scan...");
    
    return ESP_OK;
}

esp_err_t bluetooth_connect(const uint8_t *address)
{
    if (!initialized || address == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Connecting to device: %02X:%02X:%02X:%02X:%02X:%02X",
             address[0], address[1], address[2], 
             address[3], address[4], address[5]);
    
    is_connected = true;
    
    bt_connection_event_t event_data = {
        .connected = true
    };
    memcpy(event_data.device.address, address, 6);
    snprintf(event_data.device.name, sizeof(event_data.device.name), "BT Device");
    event_data.device.rssi = -45;
    
    system_event_post(bt_service_id,
                     bt_events[BT_EVENT_CONNECTED],
                     &event_data,
                     sizeof(event_data),
                     SYSTEM_EVENT_PRIORITY_HIGH);
    
    system_service_heartbeat(bt_service_id);
    
    ESP_LOGI(TAG, "✓ Bluetooth connected");
    ESP_LOGI(TAG, "  → Posted BT_EVENT_CONNECTED");
    
    return ESP_OK;
}

esp_err_t bluetooth_disconnect(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Disconnecting bluetooth...");
    
    is_connected = false;
    
    system_event_post(bt_service_id,
                     bt_events[BT_EVENT_DISCONNECTED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Bluetooth disconnected");
    
    return ESP_OK;
}

system_service_id_t bluetooth_service_get_id(void)
{
    return bt_service_id;
}
