#ifndef APP_INTERNAL_H
#define APP_INTERNAL_H

#include "system_service/app_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_REGISTRY_MAX_ENTRIES    APP_MAX_APPS

typedef struct {
    app_info_t info;
    bool registered;
    TaskHandle_t task_handle;
    app_context_t context;
} app_registry_entry_t;

typedef struct {
    app_registry_entry_t entries[APP_REGISTRY_MAX_ENTRIES];
    uint16_t count;
    SemaphoreHandle_t mutex;
    bool initialized;
} app_registry_t;

app_registry_t* app_get_registry(void);
esp_err_t app_registry_lock(void);
esp_err_t app_registry_unlock(void);
app_registry_entry_t* app_find_by_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif // APP_INTERNAL_H
