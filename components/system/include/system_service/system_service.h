#ifndef SYSTEM_SERVICE_H
#define SYSTEM_SERVICE_H

#include "system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t system_service_init(system_secure_key_t *out_secure_key);

esp_err_t system_service_deinit(system_secure_key_t secure_key);

esp_err_t system_service_start(system_secure_key_t secure_key);

esp_err_t system_service_stop(system_secure_key_t secure_key);

esp_err_t system_service_get_stats(system_secure_key_t secure_key, 
                                   uint32_t *out_total_services,
                                   uint32_t *out_total_events,
                                   uint32_t *out_total_subscriptions);

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_SERVICE_H
