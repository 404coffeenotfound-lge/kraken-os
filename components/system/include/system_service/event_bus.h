#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include "system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t system_event_register_type(const char *event_name,
                                      system_event_type_t *out_event_type);

esp_err_t system_event_subscribe(system_service_id_t service_id,
                                  system_event_type_t event_type,
                                  system_event_handler_t handler,
                                  void *user_data);

esp_err_t system_event_unsubscribe(system_service_id_t service_id,
                                    system_event_type_t event_type);

esp_err_t system_event_post(system_service_id_t sender_id,
                            system_event_type_t event_type,
                            const void *data,
                            size_t data_size,
                            system_event_priority_t priority);

esp_err_t system_event_post_async(system_service_id_t sender_id,
                                   system_event_type_t event_type,
                                   const void *data,
                                   size_t data_size,
                                   system_event_priority_t priority);

esp_err_t system_event_get_type_name(system_event_type_t event_type,
                                      char *out_name,
                                      size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // EVENT_BUS_H
