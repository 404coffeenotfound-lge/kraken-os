#ifndef APP_DOWNLOADER_H
#define APP_DOWNLOADER_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_DOWNLOAD_IDLE = 0,
    APP_DOWNLOAD_CONNECTING,
    APP_DOWNLOAD_DOWNLOADING,
    APP_DOWNLOAD_VERIFYING,
    APP_DOWNLOAD_COMPLETE,
    APP_DOWNLOAD_ERROR,
} app_download_state_t;

typedef struct {
    app_download_state_t state;
    uint32_t total_bytes;
    uint32_t downloaded_bytes;
    uint32_t progress_percent;
    char error_msg[64];
} app_download_status_t;

typedef void (*app_download_callback_t)(const app_download_status_t *status, void *user_data);

esp_err_t app_downloader_init(void);

esp_err_t app_downloader_download(const char *url, 
                                   void **out_data, 
                                   size_t *out_size,
                                   app_download_callback_t callback,
                                   void *user_data);

esp_err_t app_downloader_download_to_storage(const char *url,
                                             const char *app_name,
                                             app_download_callback_t callback,
                                             void *user_data);

esp_err_t app_downloader_get_status(app_download_status_t *status);

esp_err_t app_downloader_cancel(void);

#ifdef __cplusplus
}
#endif

#endif // APP_DOWNLOADER_H
