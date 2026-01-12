/**
 * @file error_codes.h
 * @brief Enhanced error codes for Kraken system service
 * 
 * Provides detailed, specific error codes for better error handling and debugging.
 * All error codes are in the range 0x8000-0x8FFF.
 */

#ifndef SYSTEM_SERVICE_ERROR_CODES_H
#define SYSTEM_SERVICE_ERROR_CODES_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Error Code Base
 * ============================================================================ */
#define ESP_ERR_SYSTEM_SERVICE_BASE         0x8000

/* ============================================================================
 * Service Management Errors (0x8001 - 0x8020)
 * ============================================================================ */

/** Service not found in registry */
#define ESP_ERR_SERVICE_NOT_FOUND           (ESP_ERR_SYSTEM_SERVICE_BASE + 0x01)

/** Service registry is full (max services reached) */
#define ESP_ERR_SERVICE_REGISTRY_FULL       (ESP_ERR_SYSTEM_SERVICE_BASE + 0x02)

/** Service already registered with this name */
#define ESP_ERR_SERVICE_ALREADY_REGISTERED  (ESP_ERR_SYSTEM_SERVICE_BASE + 0x03)

/** Service is in invalid state for this operation */
#define ESP_ERR_SERVICE_INVALID_STATE       (ESP_ERR_SYSTEM_SERVICE_BASE + 0x04)

/** Service dependency not satisfied */
#define ESP_ERR_SERVICE_DEPENDENCY_FAILED   (ESP_ERR_SYSTEM_SERVICE_BASE + 0x05)

/** Circular dependency detected */
#define ESP_ERR_SERVICE_CIRCULAR_DEPENDENCY (ESP_ERR_SYSTEM_SERVICE_BASE + 0x06)

/** Service watchdog timeout */
#define ESP_ERR_SERVICE_WATCHDOG_TIMEOUT    (ESP_ERR_SYSTEM_SERVICE_BASE + 0x07)

/** Service restart failed */
#define ESP_ERR_SERVICE_RESTART_FAILED      (ESP_ERR_SYSTEM_SERVICE_BASE + 0x08)

/** Service is marked as critical and cannot be stopped */
#define ESP_ERR_SERVICE_CRITICAL            (ESP_ERR_SYSTEM_SERVICE_BASE + 0x09)

/* ============================================================================
 * Event Bus Errors (0x8021 - 0x8040)
 * ============================================================================ */

/** Event type not found in registry */
#define ESP_ERR_EVENT_TYPE_NOT_FOUND        (ESP_ERR_SYSTEM_SERVICE_BASE + 0x21)

/** Event type registry is full */
#define ESP_ERR_EVENT_TYPE_REGISTRY_FULL    (ESP_ERR_SYSTEM_SERVICE_BASE + 0x22)

/** Event type already registered */
#define ESP_ERR_EVENT_TYPE_ALREADY_REGISTERED (ESP_ERR_SYSTEM_SERVICE_BASE + 0x23)

/** Event queue is full (cannot post event) */
#define ESP_ERR_EVENT_QUEUE_FULL            (ESP_ERR_SYSTEM_SERVICE_BASE + 0x24)

/** Event data size exceeds maximum allowed */
#define ESP_ERR_EVENT_DATA_TOO_LARGE        (ESP_ERR_SYSTEM_SERVICE_BASE + 0x25)

/** Event handler execution timeout */
#define ESP_ERR_EVENT_HANDLER_TIMEOUT       (ESP_ERR_SYSTEM_SERVICE_BASE + 0x26)

/** Subscription registry is full */
#define ESP_ERR_EVENT_SUBSCRIPTION_FULL     (ESP_ERR_SYSTEM_SERVICE_BASE + 0x27)

/** Subscription not found */
#define ESP_ERR_EVENT_SUBSCRIPTION_NOT_FOUND (ESP_ERR_SYSTEM_SERVICE_BASE + 0x28)

/** Event version mismatch */
#define ESP_ERR_EVENT_VERSION_MISMATCH      (ESP_ERR_SYSTEM_SERVICE_BASE + 0x29)

/* ============================================================================
 * Resource Quota Errors (0x8041 - 0x8060)
 * ============================================================================ */

/** Service exceeded event posting quota */
#define ESP_ERR_QUOTA_EVENTS_EXCEEDED       (ESP_ERR_SYSTEM_SERVICE_BASE + 0x41)

/** Service exceeded subscription quota */
#define ESP_ERR_QUOTA_SUBSCRIPTIONS_EXCEEDED (ESP_ERR_SYSTEM_SERVICE_BASE + 0x42)

/** Service exceeded memory quota */
#define ESP_ERR_QUOTA_MEMORY_EXCEEDED       (ESP_ERR_SYSTEM_SERVICE_BASE + 0x43)

/** Service exceeded data size quota */
#define ESP_ERR_QUOTA_DATA_SIZE_EXCEEDED    (ESP_ERR_SYSTEM_SERVICE_BASE + 0x44)

/* ============================================================================
 * App Manager Errors (0x8061 - 0x8080)
 * ============================================================================ */

/** App not found in registry */
#define ESP_ERR_APP_NOT_FOUND               (ESP_ERR_SYSTEM_SERVICE_BASE + 0x61)

/** App registry is full */
#define ESP_ERR_APP_REGISTRY_FULL           (ESP_ERR_SYSTEM_SERVICE_BASE + 0x62)

/** App already registered */
#define ESP_ERR_APP_ALREADY_REGISTERED      (ESP_ERR_SYSTEM_SERVICE_BASE + 0x63)

/** App context is invalid or freed */
#define ESP_ERR_APP_CONTEXT_INVALID         (ESP_ERR_SYSTEM_SERVICE_BASE + 0x64)

/** App is in invalid state for this operation */
#define ESP_ERR_APP_INVALID_STATE           (ESP_ERR_SYSTEM_SERVICE_BASE + 0x65)

/** App entry function failed */
#define ESP_ERR_APP_ENTRY_FAILED            (ESP_ERR_SYSTEM_SERVICE_BASE + 0x66)

/** App exit function failed */
#define ESP_ERR_APP_EXIT_FAILED             (ESP_ERR_SYSTEM_SERVICE_BASE + 0x67)

/** App manifest is invalid */
#define ESP_ERR_APP_INVALID_MANIFEST        (ESP_ERR_SYSTEM_SERVICE_BASE + 0x68)

/* ============================================================================
 * Security Errors (0x8081 - 0x80A0)
 * ============================================================================ */

/** Invalid secure key provided */
#define ESP_ERR_SECURITY_INVALID_KEY        (ESP_ERR_SYSTEM_SERVICE_BASE + 0x81)

/** Secure key has been invalidated */
#define ESP_ERR_SECURITY_KEY_INVALIDATED    (ESP_ERR_SYSTEM_SERVICE_BASE + 0x82)

/** Operation requires secure key but none provided */
#define ESP_ERR_SECURITY_KEY_REQUIRED       (ESP_ERR_SYSTEM_SERVICE_BASE + 0x83)

/* ============================================================================
 * Memory Errors (0x80A1 - 0x80C0)
 * ============================================================================ */

/** Memory pool allocation failed */
#define ESP_ERR_MEMORY_POOL_EXHAUSTED       (ESP_ERR_SYSTEM_SERVICE_BASE + 0xA1)

/** PSRAM allocation failed */
#define ESP_ERR_MEMORY_PSRAM_FAILED         (ESP_ERR_SYSTEM_SERVICE_BASE + 0xA2)

/** Heap fragmentation too high */
#define ESP_ERR_MEMORY_FRAGMENTATION_HIGH   (ESP_ERR_SYSTEM_SERVICE_BASE + 0xA3)

/* ============================================================================
 * System State Errors (0x80C1 - 0x80E0)
 * ============================================================================ */

/** System service not initialized */
#define ESP_ERR_SYSTEM_NOT_INITIALIZED      (ESP_ERR_SYSTEM_SERVICE_BASE + 0xC1)

/** System service already initialized */
#define ESP_ERR_SYSTEM_ALREADY_INITIALIZED  (ESP_ERR_SYSTEM_SERVICE_BASE + 0xC2)

/** System service not started */
#define ESP_ERR_SYSTEM_NOT_STARTED          (ESP_ERR_SYSTEM_SERVICE_BASE + 0xC3)

/** System service already started */
#define ESP_ERR_SYSTEM_ALREADY_STARTED      (ESP_ERR_SYSTEM_SERVICE_BASE + 0xC4)

/** Mutex lock timeout */
#define ESP_ERR_SYSTEM_MUTEX_TIMEOUT        (ESP_ERR_SYSTEM_SERVICE_BASE + 0xC5)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get human-readable error message for system service error code
 * 
 * @param err Error code
 * @return Pointer to static error message string
 */
const char* system_service_err_to_name(esp_err_t err);

/**
 * @brief Check if error code is a system service error
 * 
 * @param err Error code to check
 * @return true if error is from system service, false otherwise
 */
static inline bool is_system_service_error(esp_err_t err) {
    return (err >= ESP_ERR_SYSTEM_SERVICE_BASE && err < (ESP_ERR_SYSTEM_SERVICE_BASE + 0x1000));
}

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_SERVICE_ERROR_CODES_H
