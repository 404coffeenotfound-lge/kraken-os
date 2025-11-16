#ifndef APP_SDK_H
#define APP_SDK_H

/**
 * @file app_sdk.h
 * @brief Kraken OS App SDK - Easy-to-use header for dynamic apps
 * 
 * This header provides convenient macros and helpers for developing dynamic apps.
 * Include this in your app source files for easy access to system APIs.
 */

#include "system_service/app_manager.h"
#include "system_service/app_loader.h"
#include "system_service/event_bus.h"
#include "system_service/memory_utils.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convenience macros for apps
 */

// Logging macros (use the app context)
#define APP_LOGE(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
#define APP_LOGW(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
#define APP_LOGI(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
#define APP_LOGD(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
#define APP_LOGV(tag, format, ...) ESP_LOGV(tag, format, ##__VA_ARGS__)

// Task delay
#define APP_DELAY_MS(ms) vTaskDelay(pdMS_TO_TICKS(ms))
#define APP_DELAY_SEC(sec) vTaskDelay(pdMS_TO_TICKS((sec) * 1000))

// Memory allocation (prefer PSRAM for apps)
#define APP_ALLOC(size) APP_MALLOC(size)
#define APP_FREE(ptr) free(ptr)
#define APP_CALLOC(n, size) ({ \
    void *ptr = APP_MALLOC((n) * (size)); \
    if (ptr) memset(ptr, 0, (n) * (size)); \
    ptr; \
})

// Get app info
#define APP_NAME(ctx) ((ctx)->app_info->manifest.name)
#define APP_VERSION(ctx) ((ctx)->app_info->manifest.version)
#define APP_AUTHOR(ctx) ((ctx)->app_info->manifest.author)
#define APP_SERVICE_ID(ctx) ((ctx)->service_id)

// State management shortcuts
#define APP_SET_RUNNING(ctx) \
    (ctx)->set_state((ctx)->service_id, SYSTEM_SERVICE_STATE_RUNNING)
#define APP_SET_IDLE(ctx) \
    (ctx)->set_state((ctx)->service_id, SYSTEM_SERVICE_STATE_REGISTERED)
#define APP_HEARTBEAT(ctx) \
    (ctx)->heartbeat((ctx)->service_id)

// Event shortcuts
#define APP_REGISTER_EVENT(ctx, name, out_type) \
    (ctx)->register_event_type(name, out_type)
#define APP_POST_EVENT(ctx, type, data, size) \
    (ctx)->post_event((ctx)->service_id, type, data, size, SYSTEM_EVENT_PRIORITY_NORMAL)
#define APP_POST_EVENT_PRIORITY(ctx, type, data, size, priority) \
    (ctx)->post_event((ctx)->service_id, type, data, size, priority)
#define APP_SUBSCRIBE(ctx, type, handler, user_data) \
    (ctx)->subscribe_event((ctx)->service_id, type, handler, user_data)
#define APP_UNSUBSCRIBE(ctx, type) \
    (ctx)->unsubscribe_event((ctx)->service_id, type)

/**
 * @brief Helper: Print app banner
 */
static inline void app_print_banner(const char *tag, const char *title)
{
    APP_LOGI(tag, "╔══════════════════════════════════════════╗");
    APP_LOGI(tag, "║  %-38s  ║", title);
    APP_LOGI(tag, "╚══════════════════════════════════════════╝");
}

/**
 * @brief Helper: Print app info
 */
static inline void app_print_info(app_context_t *ctx, const char *tag)
{
    APP_LOGI(tag, "App Information:");
    APP_LOGI(tag, "  Name:       %s", APP_NAME(ctx));
    APP_LOGI(tag, "  Version:    %s", APP_VERSION(ctx));
    APP_LOGI(tag, "  Author:     %s", APP_AUTHOR(ctx));
    APP_LOGI(tag, "  Service ID: %d", APP_SERVICE_ID(ctx));
}

/**
 * @brief Helper: Check if event type matches
 */
static inline bool app_event_is_type(const system_event_t *event, system_event_type_t type)
{
    return event->event_type == type;
}

/**
 * @brief Template: Simple app structure
 * 
 * Use this template for creating new apps:
 * 
 * @code
 * #include "app_sdk.h"
 * 
 * static const char *TAG = "my_app";
 * 
 * // Event handler
 * static void my_event_handler(const system_event_t *event, void *user_data)
 * {
 *     APP_LOGI(TAG, "Received event type: %d", event->event_type);
 * }
 * 
 * // App entry point
 * esp_err_t my_app_entry(app_context_t *ctx)
 * {
 *     app_print_banner(TAG, "MY APP STARTED");
 *     app_print_info(ctx, TAG);
 *     
 *     // Set running state
 *     APP_SET_RUNNING(ctx);
 *     
 *     // Register and subscribe to events
 *     system_event_type_t my_event;
 *     APP_REGISTER_EVENT(ctx, "my_app.custom", &my_event);
 *     APP_SUBSCRIBE(ctx, my_event, my_event_handler, NULL);
 *     
 *     // Main app loop
 *     for (int i = 0; i < 10; i++) {
 *         APP_LOGI(TAG, "Loop iteration %d", i);
 *         APP_HEARTBEAT(ctx);
 *         APP_POST_EVENT(ctx, my_event, &i, sizeof(i));
 *         APP_DELAY_SEC(1);
 *     }
 *     
 *     return ESP_OK;
 * }
 * 
 * // App exit point (optional)
 * esp_err_t my_app_exit(app_context_t *ctx)
 * {
 *     APP_LOGI(TAG, "App exiting...");
 *     return ESP_OK;
 * }
 * @endcode
 */

/**
 * @brief App manifest helper macro
 * 
 * Use this to define your app manifest:
 * 
 * @code
 * KRAKEN_APP_MANIFEST(
 *     "my_app",           // name
 *     "1.0.0",            // version
 *     "Your Name",        // author
 *     my_app_entry,       // entry function
 *     my_app_exit         // exit function (or NULL)
 * );
 * @endcode
 */
#define KRAKEN_APP_MANIFEST(name, version, author, entry_fn, exit_fn) \
    const app_manifest_t app_manifest __attribute__((used)) = { \
        .name = name, \
        .version = version, \
        .author = author, \
        .entry = entry_fn, \
        .exit = exit_fn, \
        .user_data = NULL \
    }

/**
 * @brief App attribute for static data (places in PSRAM)
 */
#define APP_STATIC_DATA APP_DATA_ATTR static

/**
 * @brief Common event types that apps might use
 */
typedef enum {
    APP_EVENT_STARTED = 0,
    APP_EVENT_STOPPED,
    APP_EVENT_PAUSED,
    APP_EVENT_RESUMED,
    APP_EVENT_ERROR,
    APP_EVENT_CUSTOM_BASE = 100,  // Start custom events from 100
} app_common_event_t;

/**
 * @brief Helper: Get current tick count in milliseconds
 */
static inline uint32_t app_get_time_ms(void)
{
    return (xTaskGetTickCount() * 1000) / configTICK_RATE_HZ;
}

/**
 * @brief Helper: Get current tick count in seconds
 */
static inline uint32_t app_get_time_sec(void)
{
    return app_get_time_ms() / 1000;
}

/**
 * @brief Helper: Simple benchmark timer
 */
typedef struct {
    uint32_t start_time;
} app_timer_t;

static inline void app_timer_start(app_timer_t *timer)
{
    timer->start_time = app_get_time_ms();
}

static inline uint32_t app_timer_elapsed_ms(app_timer_t *timer)
{
    return app_get_time_ms() - timer->start_time;
}

static inline uint32_t app_timer_elapsed_sec(app_timer_t *timer)
{
    return app_timer_elapsed_ms(timer) / 1000;
}

#ifdef __cplusplus
}
#endif

#endif // APP_SDK_H
