#include "system_service/app_downloader.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include <string.h>

static const char *TAG = "app_downloader";

static app_download_status_t download_status = {0};

typedef struct {
    void *buffer;
    size_t size;
    size_t capacity;
    app_download_callback_t callback;
    void *user_data;
} download_context_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    download_context_t *ctx = (download_context_t *)evt->user_data;
    
    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP connected");
            download_status.state = APP_DOWNLOAD_DOWNLOADING;
            break;
            
        case HTTP_EVENT_ON_HEADER:
            if (strcasecmp(evt->header_key, "Content-Length") == 0) {
                download_status.total_bytes = atoi(evt->header_value);
                ESP_LOGI(TAG, "Total size: %d bytes", download_status.total_bytes);
            }
            break;
            
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Ensure buffer capacity
                if (ctx->size + evt->data_len > ctx->capacity) {
                    size_t new_capacity = ctx->capacity + evt->data_len + 4096;
                    void *new_buffer = realloc(ctx->buffer, new_capacity);
                    if (new_buffer == NULL) {
                        ESP_LOGE(TAG, "Failed to expand buffer");
                        return ESP_FAIL;
                    }
                    ctx->buffer = new_buffer;
                    ctx->capacity = new_capacity;
                }
                
                // Copy data
                memcpy((uint8_t *)ctx->buffer + ctx->size, evt->data, evt->data_len);
                ctx->size += evt->data_len;
                
                // Update progress
                download_status.downloaded_bytes = ctx->size;
                if (download_status.total_bytes > 0) {
                    download_status.progress_percent = 
                        (ctx->size * 100) / download_status.total_bytes;
                }
                
                // Callback
                if (ctx->callback != NULL) {
                    ctx->callback(&download_status, ctx->user_data);
                }
            }
            break;
            
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP download finished");
            download_status.state = APP_DOWNLOAD_COMPLETE;
            download_status.progress_percent = 100;
            
            if (ctx->callback != NULL) {
                ctx->callback(&download_status, ctx->user_data);
            }
            break;
            
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP download error");
            download_status.state = APP_DOWNLOAD_ERROR;
            snprintf(download_status.error_msg, sizeof(download_status.error_msg),
                    "HTTP error");
            
            if (ctx->callback != NULL) {
                ctx->callback(&download_status, ctx->user_data);
            }
            break;
            
        default:
            break;
    }
    
    return ESP_OK;
}

esp_err_t app_downloader_init(void)
{
    ESP_LOGI(TAG, "App downloader initialized");
    memset(&download_status, 0, sizeof(download_status));
    download_status.state = APP_DOWNLOAD_IDLE;
    return ESP_OK;
}

esp_err_t app_downloader_download(const char *url, 
                                   void **out_data, 
                                   size_t *out_size,
                                   app_download_callback_t callback,
                                   void *user_data)
{
    if (url == NULL || out_data == NULL || out_size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Downloading from: %s", url);
    
    // Reset status
    memset(&download_status, 0, sizeof(download_status));
    download_status.state = APP_DOWNLOAD_CONNECTING;
    
    // Setup context
    download_context_t ctx = {
        .buffer = malloc(4096),  // Initial buffer
        .size = 0,
        .capacity = 4096,
        .callback = callback,
        .user_data = user_data
    };
    
    if (ctx.buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate initial buffer");
        return ESP_ERR_NO_MEM;
    }
    
    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = &ctx,
        .timeout_ms = 30000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(ctx.buffer);
        return ESP_FAIL;
    }
    
    // Perform request
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP Status = %d, content_length = %d",
                status_code, ctx.size);
        
        if (status_code == 200) {
            *out_data = ctx.buffer;
            *out_size = ctx.size;
            download_status.state = APP_DOWNLOAD_COMPLETE;
        } else {
            ESP_LOGE(TAG, "HTTP request failed with status %d", status_code);
            free(ctx.buffer);
            download_status.state = APP_DOWNLOAD_ERROR;
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        free(ctx.buffer);
        download_status.state = APP_DOWNLOAD_ERROR;
    }
    
    esp_http_client_cleanup(client);
    
    return err;
}

esp_err_t app_downloader_download_to_storage(const char *url,
                                             const char *app_name,
                                             app_download_callback_t callback,
                                             void *user_data)
{
    if (url == NULL || app_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Downloading to storage: %s -> %s", url, app_name);
    
    void *data = NULL;
    size_t size = 0;
    
    esp_err_t ret = app_downloader_download(url, &data, &size, callback, user_data);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Save to storage
    // TODO: Call app_storage_save() when integrated
    ESP_LOGI(TAG, "✓ Downloaded %d bytes, ready to save to storage", size);
    
    free(data);
    
    return ESP_OK;
}

esp_err_t app_downloader_get_status(app_download_status_t *status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(status, &download_status, sizeof(app_download_status_t));
    return ESP_OK;
}

esp_err_t app_downloader_cancel(void)
{
    ESP_LOGI(TAG, "Download cancelled");
    download_status.state = APP_DOWNLOAD_IDLE;
    return ESP_OK;
}
