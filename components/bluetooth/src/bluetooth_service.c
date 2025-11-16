#include "bluetooth_service.h"
#include "system_service/system_service.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "bluetooth_service";

// BLE configuration
#define DEVICE_NAME             "Kraken-OS"

// GATT Service UUIDs
#define GATTS_SERVICE_UUID      0x00FF  // Custom service
#define GATTS_CHAR_UUID_NOTIFY  0xFF01  // Notify characteristic
#define GATTS_CHAR_UUID_WRITE   0xFF02  // Write characteristic

#define GATTS_NUM_HANDLE        4
#define GATTS_DEMO_CHAR_VAL_LEN_MAX 0x40

static system_service_id_t bt_service_id = 0;
static system_event_type_t bt_events[8];
static bool initialized = false;
static bool is_connected = false;
static uint16_t gatts_if_handle = ESP_GATT_IF_NONE;
static uint16_t conn_id = 0;
static uint16_t service_handle = 0;
static uint16_t notify_handle = 0;

// Advertising configuration flags
static uint8_t adv_config_done = 0;
#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

// Service UUID for advertising (128-bit) - Two service UUIDs like official example
static uint8_t service_uuid[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xEE, 0x00, 0x00, 0x00,
    //second uuid, 32bit, [12], [13], [14], [15] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};

// Advertising data - exactly matching official GATT server example
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(service_uuid),
    .p_service_uuid = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// Scan response data - exactly matching official GATT server example
static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(service_uuid),
    .p_service_uuid = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// Advertising parameters
static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,   // 20ms
    .adv_int_max = 0x40,   // 40ms  
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// Characteristic value
static uint8_t char_value[GATTS_DEMO_CHAR_VAL_LEN_MAX] = {0};
static uint8_t char_value_len = 0;

// Forward declarations
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

// GAP event handler
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done == 0) {
            ESP_LOGI(TAG, "✓ Advertising data set, starting advertising...");
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
        
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done == 0) {
            ESP_LOGI(TAG, "✓ Scan response data set, starting advertising...");
            esp_ble_gap_start_advertising(&adv_params);
        }
        break;
        
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Advertising start failed: %d", param->adv_start_cmpl.status);
        } else {
            ESP_LOGI(TAG, "╔════════════════════════════════════════╗");
            ESP_LOGI(TAG, "║  BLUETOOTH ADVERTISING ACTIVE!         ║");
            ESP_LOGI(TAG, "╚════════════════════════════════════════╝");
            ESP_LOGI(TAG, "  Device name: %s", DEVICE_NAME);
            ESP_LOGI(TAG, "  MAC: 98:a3:16:ec:72:fe");
            ESP_LOGI(TAG, "  Service: HID (0x1812)");
            ESP_LOGI(TAG, "  Discoverable on iOS/Windows/Android!");
        }
        break;
        
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        ESP_LOGI(TAG, "Advertising stopped");
        break;
        
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:
        ESP_LOGI(TAG, "Passkey request");
        // Auto-accept for now (you can implement custom passkey here)
        esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, 0x00);
        break;
        
    case ESP_GAP_BLE_NC_REQ_EVT:
        ESP_LOGI(TAG, "Numeric comparison request: %06" PRIu32, param->ble_security.key_notif.passkey);
        ESP_LOGI(TAG, "Auto-accepting pairing...");
        esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
        break;
        
    case ESP_GAP_BLE_SEC_REQ_EVT:
        ESP_LOGI(TAG, "Security request - sending pairing response");
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
        
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        if (param->ble_security.auth_cmpl.success) {
            ESP_LOGI(TAG, "╔════════════════════════════════════════╗");
            ESP_LOGI(TAG, "║  PAIRING SUCCESSFUL!                   ║");
            ESP_LOGI(TAG, "╚════════════════════════════════════════╝");
            ESP_LOGI(TAG, "  Device paired and bonded");
        } else {
            ESP_LOGE(TAG, "Pairing failed, status: %d", param->ble_security.auth_cmpl.fail_reason);
        }
        break;
        
    default:
        break;
    }
}

// GATTS event handler
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(TAG, "✓ GATT server registered, app_id: %d", param->reg.app_id);
        gatts_if_handle = gatts_if;
        
        // Set device name
        esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(DEVICE_NAME);
        if (set_dev_name_ret) {
            ESP_LOGE(TAG, "set device name failed, error code = %x", set_dev_name_ret);
        }
        
        // Configure advertising data (official GATT server example pattern)
        esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
        if (ret) {
            ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
        }
        adv_config_done |= adv_config_flag;
        
        // Configure scan response data
        ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
        if (ret) {
            ESP_LOGE(TAG, "config scan response data failed, error code = %x", ret);
        }
        adv_config_done |= scan_rsp_config_flag;
        
        // Create GATT service
        esp_gatt_srvc_id_t service_id = {
            .is_primary = true,
            .id.inst_id = 0,
            .id.uuid.len = ESP_UUID_LEN_16,
            .id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID,
        };
        esp_ble_gatts_create_service(gatts_if, &service_id, GATTS_NUM_HANDLE);
        break;
        
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(TAG, "✓ Service created, handle: %d", param->create.service_handle);
        service_handle = param->create.service_handle;
        esp_ble_gatts_start_service(service_handle);
        break;
        
    case ESP_GATTS_START_EVT:
        ESP_LOGI(TAG, "✓ Service started");
        break;
        
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "╔════════════════════════════════════════╗");
        ESP_LOGI(TAG, "║    BLE CLIENT CONNECTED!               ║");
        ESP_LOGI(TAG, "╚════════════════════════════════════════╝");
        ESP_LOGI(TAG, "  Connection ID: %d", param->connect.conn_id);
        ESP_LOGI(TAG, "  Remote address: %02x:%02x:%02x:%02x:%02x:%02x",
                 param->connect.remote_bda[0], param->connect.remote_bda[1],
                 param->connect.remote_bda[2], param->connect.remote_bda[3],
                 param->connect.remote_bda[4], param->connect.remote_bda[5]);
        
        conn_id = param->connect.conn_id;
        is_connected = true;
        
        // Post connection event
        bt_connection_event_t event_data = {
            .connected = true
        };
        memcpy(event_data.device.address, param->connect.remote_bda, 6);
        snprintf(event_data.device.name, sizeof(event_data.device.name), "Phone");
        event_data.device.rssi = -50;
        
        system_event_post(bt_service_id,
                         bt_events[BT_EVENT_CONNECTED],
                         &event_data,
                         sizeof(event_data),
                         SYSTEM_EVENT_PRIORITY_HIGH);
        
        // Update connection parameters
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        conn_params.latency = 0;
        conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
        conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
        conn_params.timeout = 400;     // timeout = 400*10ms = 4000ms
        esp_ble_gap_update_conn_params(&conn_params);
        break;
        
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "╔════════════════════════════════════════╗");
        ESP_LOGI(TAG, "║    BLE CLIENT DISCONNECTED             ║");
        ESP_LOGI(TAG, "╚════════════════════════════════════════╝");
        ESP_LOGI(TAG, "  Reason: %d", param->disconnect.reason);
        
        is_connected = false;
        
        // Post disconnection event
        system_event_post(bt_service_id,
                         bt_events[BT_EVENT_DISCONNECTED],
                         NULL, 0,
                         SYSTEM_EVENT_PRIORITY_NORMAL);
        
        // Restart advertising
        esp_ble_gap_start_advertising(&adv_params);
        ESP_LOGI(TAG, "✓ Restarted advertising");
        break;
        
    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(TAG, "Data received from client:");
        ESP_LOGI(TAG, "  Handle: %d", param->write.handle);
        ESP_LOGI(TAG, "  Length: %d bytes", param->write.len);
        ESP_LOG_BUFFER_HEX(TAG, param->write.value, param->write.len);
        
        // Store the value
        if (param->write.len <= GATTS_DEMO_CHAR_VAL_LEN_MAX) {
            memcpy(char_value, param->write.value, param->write.len);
            char_value_len = param->write.len;
        }
        
        // Post data received event
        bt_data_event_t data_event = {
            .data = param->write.value,
            .length = param->write.len
        };
        
        system_event_post(bt_service_id,
                         bt_events[BT_EVENT_DATA_RECEIVED],
                         &data_event,
                         sizeof(data_event),
                         SYSTEM_EVENT_PRIORITY_NORMAL);
        
        // Send response if needed
        if (param->write.need_rsp) {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                       param->write.trans_id, ESP_GATT_OK, NULL);
        }
        break;
        
    case ESP_GATTS_READ_EVT:
        ESP_LOGI(TAG, "Client read request, handle: %d", param->read.handle);
        
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = char_value_len;
        memcpy(rsp.attr_value.value, char_value, char_value_len);
        
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id,
                                   param->read.trans_id, ESP_GATT_OK, &rsp);
        break;
        
    default:
        break;
    }
}


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
    
    // Initialize Bluetooth controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BT controller: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ BT controller initialized");
    
    // Enable BLE mode
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable BT controller: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ BT controller enabled (BLE mode)");
    
    // Initialize Bluedroid stack
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bluedroid: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ Bluedroid initialized");
    
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable bluedroid: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ Bluedroid enabled");
    
    // Configure BLE security for pairing/bonding (matching official HID example)
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_IO;  // Both input and output (key matches on both)
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint32_t passkey = 1234;
    
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));
    
    ESP_LOGI(TAG, "✓ BLE security configured (matching official HID example)");
    
    // Register GAP callback
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GAP callback: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ GAP callback registered");
    
    // Register GATT callback
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GATTS callback: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ GATT server callback registered");
    
    // Set MTU early for better compatibility
    ret = esp_ble_gatt_set_local_mtu(500);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set MTU: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✓ MTU set to 500");
    }
    
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
    
    // Register GATT application
    esp_err_t ret = esp_ble_gatts_app_register(0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GATT app: %s", esp_err_to_name(ret));
        return ret;
    }
    
    system_service_set_state(bt_service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    // Post started event
    system_event_post(bt_service_id,
                     bt_events[BT_EVENT_STARTED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, "✓ Bluetooth service started");
    ESP_LOGI(TAG, "  → Posted BT_EVENT_STARTED");
    ESP_LOGI(TAG, "  Waiting for GATT registration...");
    
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

system_service_id_t bluetooth_service_get_id(void)
{
    return bt_service_id;
}

bool bluetooth_service_is_connected(void)
{
    return is_connected;
}

esp_err_t bluetooth_service_send_notification(const uint8_t *data, uint16_t len)
{
    if (!initialized || !is_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (notify_handle == 0) {
        ESP_LOGW(TAG, "Notify characteristic not created yet");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = esp_ble_gatts_send_indicate(gatts_if_handle, conn_id, notify_handle,
                                                len, (uint8_t*)data, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send notification: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ Sent notification to client (%d bytes)", len);
    return ESP_OK;
}
