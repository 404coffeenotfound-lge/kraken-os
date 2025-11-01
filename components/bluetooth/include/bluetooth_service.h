#ifndef BLUETOOTH_SERVICE_H
#define BLUETOOTH_SERVICE_H

#include "esp_err.h"
#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BT_EVENT_REGISTERED = 0,
    BT_EVENT_STARTED,
    BT_EVENT_STOPPED,
    BT_EVENT_CONNECTED,
    BT_EVENT_DISCONNECTED,
    BT_EVENT_PAIRING_REQUEST,
    BT_EVENT_DATA_RECEIVED,
    BT_EVENT_ERROR,
} bluetooth_event_id_t;

typedef struct {
    uint8_t address[6];
    char name[32];
    int8_t rssi;
} bt_device_info_t;

typedef struct {
    bt_device_info_t device;
    bool connected;
} bt_connection_event_t;

typedef struct {
    uint8_t *data;
    size_t length;
    bt_device_info_t device;
} bt_data_event_t;

esp_err_t bluetooth_service_init(void);

esp_err_t bluetooth_service_deinit(void);

esp_err_t bluetooth_service_start(void);

esp_err_t bluetooth_service_stop(void);

esp_err_t bluetooth_scan_start(void);

esp_err_t bluetooth_scan_stop(void);

esp_err_t bluetooth_connect(const uint8_t *address);

esp_err_t bluetooth_disconnect(void);

system_service_id_t bluetooth_service_get_id(void);

#ifdef __cplusplus
}
#endif

#endif // BLUETOOTH_SERVICE_H
