#include "network_service.h"
#include "system_service/system_service.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lvgl.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "network_service";

static system_service_id_t network_service_id = 0;
static system_event_type_t network_events[9];
static bool initialized = false;
static bool wifi_initialized = false;
static bool is_connected = false;
static network_connection_event_t current_status = {0};

static esp_netif_t *sta_netif = NULL;
static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;
static esp_event_handler_instance_t instance_scan_done;

/* Scan results storage */
static wifi_ap_record_t *scan_records = NULL;
static uint16_t scan_count = 0;
static bool scan_in_progress = false;

/* Comparison function for qsort - sort by RSSI descending */
static int compare_rssi(const void *a, const void *b)
{
    const network_wifi_info_t *net_a = (const network_wifi_info_t *)a;
    const network_wifi_info_t *net_b = (const network_wifi_info_t *)b;
    return net_b->rssi - net_a->rssi;  // Descending order (stronger signal first)
}

/* Convert ESP WiFi auth mode to our auth mode */
static network_auth_mode_t convert_auth_mode(wifi_auth_mode_t esp_auth)
{
    switch (esp_auth) {
        case WIFI_AUTH_OPEN:
            return NETWORK_AUTH_OPEN;
        case WIFI_AUTH_WEP:
            return NETWORK_AUTH_WEP;
        case WIFI_AUTH_WPA_PSK:
            return NETWORK_AUTH_WPA_PSK;
        case WIFI_AUTH_WPA2_PSK:
            return NETWORK_AUTH_WPA2_PSK;
        case WIFI_AUTH_WPA_WPA2_PSK:
            return NETWORK_AUTH_WPA_WPA2_PSK;
        case WIFI_AUTH_WPA3_PSK:
            return NETWORK_AUTH_WPA3_PSK;
        default:
            return NETWORK_AUTH_WPA2_PSK;
    }
}

/* WiFi scan done handler */
static void wifi_scan_done_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        scan_in_progress = false;
        ESP_LOGI(TAG, "WiFi scan completed");
        
        // Allocate scan result on heap to avoid stack overflow in sys_evt task
        network_scan_result_t *scan_result = malloc(sizeof(network_scan_result_t));
        if (scan_result == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for scan results");
            return;
        }
        
        // Get and display scan results
        if (network_scan_wifi(scan_result) == ESP_OK) {
            ESP_LOGI(TAG, "Found %d WiFi networks:", scan_result->count);
            for (int i = 0; i < scan_result->count; i++) {
                const char *auth_str = "OPEN";
                switch (scan_result->networks[i].auth_mode) {
                    case NETWORK_AUTH_WEP: auth_str = "WEP"; break;
                    case NETWORK_AUTH_WPA_PSK: auth_str = "WPA"; break;
                    case NETWORK_AUTH_WPA2_PSK: auth_str = "WPA2"; break;
                    case NETWORK_AUTH_WPA_WPA2_PSK: auth_str = "WPA/WPA2"; break;
                    case NETWORK_AUTH_WPA3_PSK: auth_str = "WPA3"; break;
                    default: break;
                }
                
                ESP_LOGI(TAG, "  [%d] %s (RSSI: %d dBm, Ch: %d, %s)%s",
                         i + 1,
                         scan_result->networks[i].ssid,
                         scan_result->networks[i].rssi,
                         scan_result->networks[i].channel,
                         auth_str,
                         scan_result->networks[i].connected ? " [CONNECTED]" : "");
            }
        }
        
        // Free heap allocation
        free(scan_result);
        
        // Post scan done event (without data to avoid size limit)
        system_event_post(network_service_id,
                         network_events[NETWORK_EVENT_SCAN_DONE],
                         NULL, 0,
                         SYSTEM_EVENT_PRIORITY_NORMAL);
    }
}

/* WiFi event handler */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi station started");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected");
        
        is_connected = false;
        current_status.wifi.connected = false;
        memset(&current_status.ip_info, 0, sizeof(current_status.ip_info));
        
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
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*) event_data;
        
        ESP_LOGI(TAG, "WiFi connected to AP SSID:%s", event->ssid);
        
        strncpy(current_status.wifi.ssid, (char*)event->ssid, NETWORK_SSID_MAX_LEN - 1);
        current_status.wifi.ssid[NETWORK_SSID_MAX_LEN - 1] = '\0';
        current_status.wifi.channel = event->channel;
        current_status.wifi.connected = true;
        
        // Get RSSI
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            current_status.wifi.rssi = ap_info.rssi;
            current_status.wifi.auth_mode = convert_auth_mode(ap_info.authmode);
        }
        
        is_connected = true;
        
        // Post connected event
        system_event_post(network_service_id,
                         network_events[NETWORK_EVENT_CONNECTED],
                         &current_status,
                         sizeof(current_status),
                         SYSTEM_EVENT_PRIORITY_HIGH);
    }
}

/* IP event handler */
static void ip_event_handler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        
        current_status.ip_info.ip = event->ip_info.ip.addr;
        current_status.ip_info.netmask = event->ip_info.netmask.addr;
        current_status.ip_info.gateway = event->ip_info.gw.addr;
        
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        
        // Post IP assigned event
        system_event_post(network_service_id,
                         network_events[NETWORK_EVENT_IP_ASSIGNED],
                         &current_status.ip_info,
                         sizeof(current_status.ip_info),
                         SYSTEM_EVENT_PRIORITY_NORMAL);
        
        system_service_heartbeat(network_service_id);
    }
}

/* Initialize WiFi subsystem */
static esp_err_t init_wifi(void)
{
    if (wifi_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing WiFi subsystem...");
    
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create default WiFi station
    sta_netif = esp_netif_create_default_wifi_sta();
    
    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &ip_event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        WIFI_EVENT_SCAN_DONE,
                                                        &wifi_scan_done_handler,
                                                        NULL,
                                                        &instance_scan_done));
    
    // Set WiFi mode to station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    
    wifi_initialized = true;
    ESP_LOGI(TAG, "✓ WiFi subsystem initialized");
    
    return ESP_OK;
}

/* Event handler for menu clicks */
static lv_obj_t *s_network_ui = NULL;
static system_event_type_t s_menu_network_event = SYSTEM_EVENT_TYPE_INVALID;
static bool s_network_ui_pending = false;  // Flag to trigger UI creation

static void network_menu_event_handler(const system_event_t *event, void *user_data)
{
    if (event->event_type == s_menu_network_event) {
        ESP_LOGI(TAG, "Network menu clicked - scheduling UI load");
        
        // Just set a flag - don't create UI here
        s_network_ui_pending = true;
        
    } else if (event->event_type == network_events[NETWORK_EVENT_SCAN_DONE]) {
        ESP_LOGI(TAG, "WiFi scan done - updating UI");
        
        if (s_network_ui) {
            extern void network_ui_update_wifi_list(lv_obj_t *ui);
            network_ui_update_wifi_list(s_network_ui);
        }
    }
}

/* Timer callback to create and load network UI in LVGL context */
static void network_ui_loader_timer_cb(lv_timer_t *timer)
{
    if (!s_network_ui_pending) {
        return;
    }
    
    ESP_LOGI(TAG, "Creating network UI in LVGL context");
    
    extern esp_err_t display_service_load_screen(lv_obj_t *content);
    extern lv_obj_t* display_service_get_main_screen(void);
    extern lv_obj_t* network_ui_create(lv_obj_t *parent);
    
    lv_obj_t *main_screen = display_service_get_main_screen();
    if (main_screen) {
        // Note: Don't destroy previous UI here - nav_pop() handles cleanup when user goes back
        // Just create new UI and let the navigation stack manage lifecycle
        s_network_ui = network_ui_create(main_screen);
        if (s_network_ui) {
            display_service_load_screen(s_network_ui);
        }
    }
    
    s_network_ui_pending = false;
}

esp_err_t network_service_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Network service already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing network service...");
    
    // Initialize NVS (required for WiFi)
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
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
        "network.scan_done",
        "network.error"
    };
    
    for (int i = 0; i < 9; i++) {
        ret = system_event_register_type(event_names[i], &network_events[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register event type '%s'", event_names[i]);
            return ret;
        }
    }
    
    ESP_LOGI(TAG, "✓ Registered %d event types", 9);
    
    // Subscribe to menu events (get event type registered by display service)
    system_event_register_type("menu.network_clicked", &s_menu_network_event);
    ret = system_event_subscribe(network_service_id, s_menu_network_event, network_menu_event_handler, NULL);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Subscribed to menu.network_clicked event");
    }
    
    // Subscribe to own scan_done event to update UI
    ret = system_event_subscribe(network_service_id, network_events[NETWORK_EVENT_SCAN_DONE], network_menu_event_handler, NULL);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Subscribed to network.scan_done event");
    }
    
    // Initialize WiFi
    ret = init_wifi();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi");
        return ret;
    }
    
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
    
    // Disconnect if connected
    if (is_connected) {
        network_disconnect_wifi();
    }
    
    // Stop WiFi
    if (wifi_initialized) {
        esp_wifi_stop();
        esp_wifi_deinit();
        
        // Unregister event handlers
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
        
        esp_netif_destroy_default_wifi(sta_netif);
        
        wifi_initialized = false;
    }
    
    // Free scan records if allocated
    if (scan_records) {
        free(scan_records);
        scan_records = NULL;
    }
    
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
    
    // Create periodic timer to check for pending UI operations
    // This runs in LVGL task context
    extern lv_timer_t* lv_timer_create(lv_timer_cb_t timer_xcb, uint32_t period, void *user_data);
    lv_timer_create(network_ui_loader_timer_cb, 50, NULL);  // Check every 50ms
    
    system_service_set_state(network_service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    // Post started event
    system_event_post(network_service_id,
                     network_events[NETWORK_EVENT_STARTED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Network service started");
    ESP_LOGI(TAG, "  → Posted NETWORK_EVENT_STARTED");
    
    // Disabled: Automatically scan for WiFi networks on startup
    // Uncomment below to enable auto-scan on boot
    /*
    ESP_LOGI(TAG, "Starting initial WiFi scan (async)...");
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_PASSIVE,  // Use passive scan for BT coexistence
        // Don't set scan_time - let WiFi use default timing for BT coexistence
    };
    
    scan_in_progress = true;
    esp_err_t scan_ret = esp_wifi_scan_start(&scan_config, false);  // false = async
    if (scan_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start WiFi scan: %s", esp_err_to_name(scan_ret));
        scan_in_progress = false;
    }
    */
    
    return ESP_OK;
}

esp_err_t network_service_stop(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Stopping network service...");
    
    // Disconnect if connected
    if (is_connected) {
        network_disconnect_wifi();
    }
    
    system_service_set_state(network_service_id, SYSTEM_SERVICE_STATE_STOPPING);
    
    // Post stopped event
    system_event_post(network_service_id,
                     network_events[NETWORK_EVENT_STOPPED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Network service stopped");
    
    return ESP_OK;
}

esp_err_t network_scan_wifi(network_scan_result_t *result)
{
    if (!initialized || !wifi_initialized || result == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Scanning for WiFi networks...");
    
    memset(result, 0, sizeof(network_scan_result_t));
    
    // If scan already in progress, wait for it
    if (scan_in_progress) {
        ESP_LOGI(TAG, "Scan already in progress, waiting...");
        // Wait up to 15 seconds for scan to complete
        for (int i = 0; i < 150 && scan_in_progress; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (scan_in_progress) {
            ESP_LOGW(TAG, "Scan timeout");
            return ESP_ERR_TIMEOUT;
        }
    } else {
        // Start new scan (blocking)
        wifi_scan_config_t scan_config = {
            .ssid = NULL,
            .bssid = NULL,
            .channel = 0,
            .show_hidden = false,
            .scan_type = WIFI_SCAN_TYPE_ACTIVE,
            .scan_time.active.min = 100,
            .scan_time.active.max = 300,
        };
        
        scan_in_progress = true;
        esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
        scan_in_progress = false;
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start WiFi scan: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    
    // Get scan results
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count == 0) {
        ESP_LOGW(TAG, "No WiFi networks found");
        return ESP_OK;
    }
    
    // Allocate memory for scan records
    if (scan_records) {
        free(scan_records);
    }
    scan_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (scan_records == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for scan results");
        return ESP_ERR_NO_MEM;
    }
    
    // Get AP records
    scan_count = ap_count;
    esp_err_t ret = esp_wifi_scan_get_ap_records(&scan_count, scan_records);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get scan records: %s", esp_err_to_name(ret));
        free(scan_records);
        scan_records = NULL;
        return ret;
    }
    
    // Convert to our format
    result->count = (scan_count > NETWORK_MAX_SCAN_RESULTS) ? NETWORK_MAX_SCAN_RESULTS : scan_count;
    
    for (int i = 0; i < result->count; i++) {
        strncpy(result->networks[i].ssid, (char*)scan_records[i].ssid, NETWORK_SSID_MAX_LEN - 1);
        result->networks[i].ssid[NETWORK_SSID_MAX_LEN - 1] = '\0';
        result->networks[i].rssi = scan_records[i].rssi;
        result->networks[i].channel = scan_records[i].primary;
        result->networks[i].auth_mode = convert_auth_mode(scan_records[i].authmode);
        result->networks[i].connected = false;
        
        // Mark current network as connected
        if (is_connected && strcmp(result->networks[i].ssid, current_status.wifi.ssid) == 0) {
            result->networks[i].connected = true;
        }
    }
    
    // Sort networks by signal strength (RSSI descending)
    qsort(result->networks, result->count, sizeof(network_wifi_info_t), compare_rssi);
    
    // Note: NETWORK_EVENT_SCAN_DONE is posted by wifi_scan_done_handler
    // We don't post scan results here to avoid event bus size limit (512 bytes)
    
    system_service_heartbeat(network_service_id);
    
    ESP_LOGI(TAG, "✓ WiFi scan complete: %d networks found", result->count);
    for (int i = 0; i < result->count; i++) {
        const char *auth_str = "OPEN";
        switch (result->networks[i].auth_mode) {
            case NETWORK_AUTH_WEP: auth_str = "WEP"; break;
            case NETWORK_AUTH_WPA_PSK: auth_str = "WPA"; break;
            case NETWORK_AUTH_WPA2_PSK: auth_str = "WPA2"; break;
            case NETWORK_AUTH_WPA_WPA2_PSK: auth_str = "WPA/WPA2"; break;
            case NETWORK_AUTH_WPA3_PSK: auth_str = "WPA3"; break;
            default: break;
        }
        
        ESP_LOGI(TAG, "  [%d] %s (RSSI: %d dBm, Ch: %d, %s)%s",
                 i + 1,
                 result->networks[i].ssid,
                 result->networks[i].rssi,
                 result->networks[i].channel,
                 auth_str,
                 result->networks[i].connected ? " [CONNECTED]" : "");
    }
    
    return ESP_OK;
}

esp_err_t network_connect_wifi(const char *ssid, const char *password)
{
    if (!initialized || !wifi_initialized || ssid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    
    // Disconnect from current network if connected
    if (is_connected) {
        ESP_LOGI(TAG, "Disconnecting from current network first...");
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Configure WiFi connection
    wifi_config_t wifi_config = {0};
    
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password != NULL) {
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }
    
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // Connect to AP
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect to WiFi: %s", esp_err_to_name(ret));
        
        // Post error event
        system_event_post(network_service_id,
                         network_events[NETWORK_EVENT_ERROR],
                         NULL, 0,
                         SYSTEM_EVENT_PRIORITY_NORMAL);
        return ret;
    }
    
    ESP_LOGI(TAG, "WiFi connection initiated...");
    
    return ESP_OK;
}

esp_err_t network_disconnect_wifi(void)
{
    if (!initialized || !wifi_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!is_connected) {
        ESP_LOGW(TAG, "Not connected to any network");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Disconnecting from WiFi: %s", current_status.wifi.ssid);
    
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "WiFi disconnection initiated...");
    
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

bool network_is_connected(void)
{
    return is_connected;
}

system_service_id_t network_service_get_id(void)
{
    return network_service_id;
}
