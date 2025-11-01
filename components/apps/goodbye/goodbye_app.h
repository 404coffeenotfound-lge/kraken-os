#ifndef GOODBYE_APP_H
#define GOODBYE_APP_H

#include "system_service/app_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const app_manifest_t goodbye_app_manifest;

esp_err_t goodbye_app_entry(app_context_t *ctx);

esp_err_t goodbye_app_exit(app_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // GOODBYE_APP_H
