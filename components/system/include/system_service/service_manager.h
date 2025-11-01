#ifndef SERVICE_MANAGER_H
#define SERVICE_MANAGER_H

#include "system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t system_service_register(const char *service_name,
                                   void *service_context,
                                   system_service_id_t *out_service_id);

esp_err_t system_service_unregister(system_service_id_t service_id);

esp_err_t system_service_set_state(system_service_id_t service_id,
                                    system_service_state_t state);

esp_err_t system_service_get_state(system_service_id_t service_id,
                                    system_service_state_t *out_state);

esp_err_t system_service_heartbeat(system_service_id_t service_id);

esp_err_t system_service_get_info(system_service_id_t service_id,
                                   system_service_info_t *out_info);

esp_err_t system_service_list_all(system_service_info_t *out_services,
                                   uint32_t max_count,
                                   uint32_t *out_count);

#ifdef __cplusplus
}
#endif

#endif // SERVICE_MANAGER_H
