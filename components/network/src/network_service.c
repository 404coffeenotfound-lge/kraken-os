#include "network_service.h"
#include "system_service/system_service.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "network_service";

static system_service_id_t network_service_id = 0;
static system_event_type_t network_events[8];
static bool initialized = false;
static bool is_connected = false;
static network_connection_event_t current_status = {0};

esp_err_t network_service_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Network service already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing network service...");
    
    // Register with system service
    ret = system_service_register("network_service", NULL, &network_service_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register with system service: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ Registered with system service (ID: %d)", network_service_id);
    
    // Register event types
    const char *event_names[] = {
        "network.registered",
        "network.started",
        "network.stopped",
        "network.connected",
        "network.disconnected",
        "network.ip_assigned",
        "network.ip_lost",
        "network.error"
    };
    
    for (int i = 0; i < 8; i++) {
        ret = system_event_register_type(event_names[i], &network_events[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register event type '%s'", event_names[i]);
            return ret;
        }
    }
    
    ESP_LOGI(TAG, "✓ Registered %d event types", 8);
    
    // Set service state
    system_service_set_state(network_service_id, SYSTEM_SERVICE_STATE_REGISTERED);
    
    initialized = true;
    
    // Post registration event
    system_event_post(network_service_id,
                     network_events[NETWORK_EVENT_REGISTERED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Network service initialized successfully");
    ESP_LOGI(TAG, "  → Posted NETWORK_EVENT_REGISTERED");
    
    return ESP_OK;
}

esp_err_t network_service_deinit(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing network service...");
    
    system_service_unregister(network_service_id);
    initialized = false;
    
    ESP_LOGI(TAG, "✓ Network service deinitialized");
    
    return ESP_OK;
}

esp_err_t network_service_start(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting network service...");
    
    system_service_set_state(network_service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    // Post started event
    system_event_post(network_service_id,
                     network_events[NETWORK_EVENT_STARTED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Network service started");
    ESP_LOGI(TAG, "  → Posted NETWORK_EVENT_STARTED");
    
    return ESP_OK;
}

esp_err_t network_service_stop(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Stopping network service...");
    
    system_service_set_state(network_service_id, SYSTEM_SERVICE_STATE_STOPPING);
    
    // Post stopped event
    system_event_post(network_service_id,
                     network_events[NETWORK_EVENT_STOPPED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Network service stopped");
    
    return ESP_OK;
}

esp_err_t network_connect_wifi(const char *ssid, const char *password)
{
    if (!initialized || ssid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    
    // Simulate connection
    strncpy(current_status.wifi.ssid, ssid, sizeof(current_status.wifi.ssid) - 1);
    current_status.wifi.rssi = -55;
    current_status.wifi.connected = true;
    
    // Simulate IP assignment
    current_status.ip_info.ip = 0xC0A80164;      // 192.168.1.100
    current_status.ip_info.netmask = 0xFFFFFF00; // 255.255.255.0
    current_status.ip_info.gateway = 0xC0A80101; // 192.168.1.1
    
    is_connected = true;
    
    // Post connected event
    system_event_post(network_service_id,
                     network_events[NETWORK_EVENT_CONNECTED],
                     &current_status,
                     sizeof(current_status),
                     SYSTEM_EVENT_PRIORITY_HIGH);
    
    // Post IP assigned event
    system_event_post(network_service_id,
                     network_events[NETWORK_EVENT_IP_ASSIGNED],
                     &current_status.ip_info,
                     sizeof(current_status.ip_info),
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    system_service_heartbeat(network_service_id);
    
    ESP_LOGI(TAG, "✓ Network connected to %s", ssid);
    ESP_LOGI(TAG, "  → Posted NETWORK_EVENT_CONNECTED");
    ESP_LOGI(TAG, "  → Posted NETWORK_EVENT_IP_ASSIGNED");
    ESP_LOGI(TAG, "  IP: %d.%d.%d.%d",
             (uint8_t)(current_status.ip_info.ip & 0xFF),
             (uint8_t)((current_status.ip_info.ip >> 8) & 0xFF),
             (uint8_t)((current_status.ip_info.ip >> 16) & 0xFF),
             (uint8_t)((current_status.ip_info.ip >> 24) & 0xFF));
    
    return ESP_OK;
}

esp_err_t network_disconnect_wifi(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Disconnecting WiFi...");
    
    is_connected = false;
    current_status.wifi.connected = false;
    
    // Post disconnected event
    system_event_post(network_service_id,
                     network_events[NETWORK_EVENT_DISCONNECTED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    // Post IP lost event
    system_event_post(network_service_id,
                     network_events[NETWORK_EVENT_IP_LOST],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Network disconnected");
    
    return ESP_OK;
}

esp_err_t network_get_status(network_connection_event_t *status)
{
    if (!initialized || status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(status, &current_status, sizeof(network_connection_event_t));
    return ESP_OK;
}

system_service_id_t network_service_get_id(void)
{
    return network_service_id;
}
