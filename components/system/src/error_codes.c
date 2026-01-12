/**
 * @file error_codes.c
 * @brief Implementation of error code to string conversion
 */

#include "system_service/error_codes.h"

const char* system_service_err_to_name(esp_err_t err)
{
    switch (err) {
        /* Service Management Errors */
        case ESP_ERR_SERVICE_NOT_FOUND:
            return "ESP_ERR_SERVICE_NOT_FOUND";
        case ESP_ERR_SERVICE_REGISTRY_FULL:
            return "ESP_ERR_SERVICE_REGISTRY_FULL";
        case ESP_ERR_SERVICE_ALREADY_REGISTERED:
            return "ESP_ERR_SERVICE_ALREADY_REGISTERED";
        case ESP_ERR_SERVICE_INVALID_STATE:
            return "ESP_ERR_SERVICE_INVALID_STATE";
        case ESP_ERR_SERVICE_DEPENDENCY_FAILED:
            return "ESP_ERR_SERVICE_DEPENDENCY_FAILED";
        case ESP_ERR_SERVICE_CIRCULAR_DEPENDENCY:
            return "ESP_ERR_SERVICE_CIRCULAR_DEPENDENCY";
        case ESP_ERR_SERVICE_WATCHDOG_TIMEOUT:
            return "ESP_ERR_SERVICE_WATCHDOG_TIMEOUT";
        case ESP_ERR_SERVICE_RESTART_FAILED:
            return "ESP_ERR_SERVICE_RESTART_FAILED";
        case ESP_ERR_SERVICE_CRITICAL:
            return "ESP_ERR_SERVICE_CRITICAL";

        /* Event Bus Errors */
        case ESP_ERR_EVENT_TYPE_NOT_FOUND:
            return "ESP_ERR_EVENT_TYPE_NOT_FOUND";
        case ESP_ERR_EVENT_TYPE_REGISTRY_FULL:
            return "ESP_ERR_EVENT_TYPE_REGISTRY_FULL";
        case ESP_ERR_EVENT_TYPE_ALREADY_REGISTERED:
            return "ESP_ERR_EVENT_TYPE_ALREADY_REGISTERED";
        case ESP_ERR_EVENT_QUEUE_FULL:
            return "ESP_ERR_EVENT_QUEUE_FULL";
        case ESP_ERR_EVENT_DATA_TOO_LARGE:
            return "ESP_ERR_EVENT_DATA_TOO_LARGE";
        case ESP_ERR_EVENT_HANDLER_TIMEOUT:
            return "ESP_ERR_EVENT_HANDLER_TIMEOUT";
        case ESP_ERR_EVENT_SUBSCRIPTION_FULL:
            return "ESP_ERR_EVENT_SUBSCRIPTION_FULL";
        case ESP_ERR_EVENT_SUBSCRIPTION_NOT_FOUND:
            return "ESP_ERR_EVENT_SUBSCRIPTION_NOT_FOUND";
        case ESP_ERR_EVENT_VERSION_MISMATCH:
            return "ESP_ERR_EVENT_VERSION_MISMATCH";

        /* Resource Quota Errors */
        case ESP_ERR_QUOTA_EVENTS_EXCEEDED:
            return "ESP_ERR_QUOTA_EVENTS_EXCEEDED";
        case ESP_ERR_QUOTA_SUBSCRIPTIONS_EXCEEDED:
            return "ESP_ERR_QUOTA_SUBSCRIPTIONS_EXCEEDED";
        case ESP_ERR_QUOTA_MEMORY_EXCEEDED:
            return "ESP_ERR_QUOTA_MEMORY_EXCEEDED";
        case ESP_ERR_QUOTA_DATA_SIZE_EXCEEDED:
            return "ESP_ERR_QUOTA_DATA_SIZE_EXCEEDED";

        /* App Manager Errors */
        case ESP_ERR_APP_NOT_FOUND:
            return "ESP_ERR_APP_NOT_FOUND";
        case ESP_ERR_APP_REGISTRY_FULL:
            return "ESP_ERR_APP_REGISTRY_FULL";
        case ESP_ERR_APP_ALREADY_REGISTERED:
            return "ESP_ERR_APP_ALREADY_REGISTERED";
        case ESP_ERR_APP_CONTEXT_INVALID:
            return "ESP_ERR_APP_CONTEXT_INVALID";
        case ESP_ERR_APP_INVALID_STATE:
            return "ESP_ERR_APP_INVALID_STATE";
        case ESP_ERR_APP_ENTRY_FAILED:
            return "ESP_ERR_APP_ENTRY_FAILED";
        case ESP_ERR_APP_EXIT_FAILED:
            return "ESP_ERR_APP_EXIT_FAILED";
        case ESP_ERR_APP_INVALID_MANIFEST:
            return "ESP_ERR_APP_INVALID_MANIFEST";

        /* Security Errors */
        case ESP_ERR_SECURITY_INVALID_KEY:
            return "ESP_ERR_SECURITY_INVALID_KEY";
        case ESP_ERR_SECURITY_KEY_INVALIDATED:
            return "ESP_ERR_SECURITY_KEY_INVALIDATED";
        case ESP_ERR_SECURITY_KEY_REQUIRED:
            return "ESP_ERR_SECURITY_KEY_REQUIRED";

        /* Memory Errors */
        case ESP_ERR_MEMORY_POOL_EXHAUSTED:
            return "ESP_ERR_MEMORY_POOL_EXHAUSTED";
        case ESP_ERR_MEMORY_PSRAM_FAILED:
            return "ESP_ERR_MEMORY_PSRAM_FAILED";
        case ESP_ERR_MEMORY_FRAGMENTATION_HIGH:
            return "ESP_ERR_MEMORY_FRAGMENTATION_HIGH";

        /* System State Errors */
        case ESP_ERR_SYSTEM_NOT_INITIALIZED:
            return "ESP_ERR_SYSTEM_NOT_INITIALIZED";
        case ESP_ERR_SYSTEM_ALREADY_INITIALIZED:
            return "ESP_ERR_SYSTEM_ALREADY_INITIALIZED";
        case ESP_ERR_SYSTEM_NOT_STARTED:
            return "ESP_ERR_SYSTEM_NOT_STARTED";
        case ESP_ERR_SYSTEM_ALREADY_STARTED:
            return "ESP_ERR_SYSTEM_ALREADY_STARTED";
        case ESP_ERR_SYSTEM_MUTEX_TIMEOUT:
            return "ESP_ERR_SYSTEM_MUTEX_TIMEOUT";

        default:
            // Fall back to standard ESP-IDF error names
            return esp_err_to_name(err);
    }
}
