#ifndef NETWORK_SERVICE_H
#define NETWORK_SERVICE_H

#include "esp_err.h"
#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NETWORK_MAX_SCAN_RESULTS 20
#define NETWORK_SSID_MAX_LEN 33
#define NETWORK_PASSWORD_MAX_LEN 64

typedef enum {
    NETWORK_EVENT_REGISTERED = 0,
    NETWORK_EVENT_STARTED,
    NETWORK_EVENT_STOPPED,
    NETWORK_EVENT_CONNECTED,
    NETWORK_EVENT_DISCONNECTED,
    NETWORK_EVENT_IP_ASSIGNED,
    NETWORK_EVENT_IP_LOST,
    NETWORK_EVENT_SCAN_DONE,
    NETWORK_EVENT_ERROR,
} network_event_id_t;

typedef enum {
    NETWORK_AUTH_OPEN = 0,
    NETWORK_AUTH_WEP,
    NETWORK_AUTH_WPA_PSK,
    NETWORK_AUTH_WPA2_PSK,
    NETWORK_AUTH_WPA_WPA2_PSK,
    NETWORK_AUTH_WPA3_PSK,
} network_auth_mode_t;

typedef struct {
    char ssid[NETWORK_SSID_MAX_LEN];
    int8_t rssi;
    uint8_t channel;
    network_auth_mode_t auth_mode;
    bool connected;
} network_wifi_info_t;

typedef struct {
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
} network_ip_info_t;

typedef struct {
    network_wifi_info_t wifi;
    network_ip_info_t ip_info;
} network_connection_event_t;

typedef struct {
    network_wifi_info_t networks[NETWORK_MAX_SCAN_RESULTS];
    uint16_t count;
} network_scan_result_t;

// Service lifecycle
esp_err_t network_service_init(void);
esp_err_t network_service_deinit(void);
esp_err_t network_service_start(void);
esp_err_t network_service_stop(void);

// WiFi management
esp_err_t network_scan_wifi(network_scan_result_t *result);
esp_err_t network_connect_wifi(const char *ssid, const char *password);
esp_err_t network_disconnect_wifi(void);
esp_err_t network_get_status(network_connection_event_t *status);
bool network_is_connected(void);

// Service info
system_service_id_t network_service_get_id(void);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_SERVICE_H
