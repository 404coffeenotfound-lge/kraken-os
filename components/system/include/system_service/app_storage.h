#ifndef APP_STORAGE_H
#define APP_STORAGE_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_STORAGE_BASE_PATH   "/storage/apps"
#define APP_STORAGE_MAX_SIZE    (512 * 1024)  // 512KB max per app

typedef struct {
    char name[64];
    char path[256];
    uint32_t size;
    uint32_t crc32;
    uint32_t install_time;
} app_storage_entry_t;

esp_err_t app_storage_init(void);

esp_err_t app_storage_mount(void);

esp_err_t app_storage_unmount(void);

esp_err_t app_storage_save(const char *app_name, const void *data, size_t size);

esp_err_t app_storage_load(const char *app_name, void **out_data, size_t *out_size);

esp_err_t app_storage_delete(const char *app_name);

esp_err_t app_storage_exists(const char *app_name, bool *exists);

esp_err_t app_storage_list(app_storage_entry_t *entries, size_t max_count, size_t *out_count);

esp_err_t app_storage_get_info(const char *app_name, app_storage_entry_t *info);

esp_err_t app_storage_get_free_space(size_t *free_bytes);

#ifdef __cplusplus
}
#endif

#endif // APP_STORAGE_H
