#ifndef NETWORK_SERVICE_H
#define NETWORK_SERVICE_H

#include "esp_err.h"
#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NETWORK_EVENT_REGISTERED = 0,
    NETWORK_EVENT_STARTED,
    NETWORK_EVENT_STOPPED,
    NETWORK_EVENT_CONNECTED,
    NETWORK_EVENT_DISCONNECTED,
    NETWORK_EVENT_IP_ASSIGNED,
    NETWORK_EVENT_IP_LOST,
    NETWORK_EVENT_ERROR,
} network_event_id_t;

typedef struct {
    char ssid[33];
    int8_t rssi;
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

esp_err_t network_service_init(void);

esp_err_t network_service_deinit(void);

esp_err_t network_service_start(void);

esp_err_t network_service_stop(void);

esp_err_t network_connect_wifi(const char *ssid, const char *password);

esp_err_t network_disconnect_wifi(void);

esp_err_t network_get_status(network_connection_event_t *status);

system_service_id_t network_service_get_id(void);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_SERVICE_H
