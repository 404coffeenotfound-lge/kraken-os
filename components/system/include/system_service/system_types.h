/**
 * @file system_types.h
 * @brief Core type definitions for Kraken system service
 * 
 * This file contains all fundamental types used across the system service.
 * These types are shared between all modules and form the foundation of the API.
 * 
 * @note This file has no dependencies on other system service headers.
 */

#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/** Maximum length of service/event names */
#define SYSTEM_SERVICE_MAX_NAME_LEN     32

/** Maximum number of services (from Kconfig) */
#define SYSTEM_SERVICE_MAX_SERVICES     CONFIG_SYSTEM_SERVICE_MAX_SERVICES

/** Maximum number of event types (from Kconfig) */
#define SYSTEM_SERVICE_MAX_EVENT_TYPES  CONFIG_SYSTEM_SERVICE_MAX_EVENT_TYPES

/** Maximum number of subscribers (from Kconfig) */
#define SYSTEM_SERVICE_MAX_SUBSCRIBERS  CONFIG_SYSTEM_SERVICE_MAX_SUBSCRIBERS

/** Maximum event data size (from Kconfig) */
#define SYSTEM_MAX_DATA_SIZE            CONFIG_SYSTEM_SERVICE_MAX_DATA_SIZE

/** Event queue size (from Kconfig) */
#define SYSTEM_EVENT_QUEUE_SIZE         CONFIG_SYSTEM_SERVICE_EVENT_QUEUE_SIZE

/** Maximum service dependencies */
#define SYSTEM_SERVICE_MAX_DEPENDENCIES 8

/** Magic number for structure validation */
#define SYSTEM_SERVICE_MAGIC            0x4B52414B  // "KRAK"

/* ============================================================================
 * Basic Type Definitions
 * ============================================================================ */

/** Secure key type for protected operations */
typedef uint32_t system_secure_key_t;

/** Service identifier type */
typedef uint16_t system_service_id_t;

/** Event type identifier */
typedef uint16_t system_event_type_t;

/** Invalid service ID constant */
#define SYSTEM_SERVICE_ID_INVALID       0xFFFF

/** Invalid event type constant */
#define SYSTEM_EVENT_TYPE_INVALID       0xFFFF

/* ============================================================================
 * Service State Enumeration
 * ============================================================================ */

/**
 * @brief Service lifecycle states
 * 
 * State transitions:
 * UNREGISTERED -> REGISTERED -> RUNNING <-> PAUSED -> STOPPING -> UNREGISTERED
 *                                  |
 *                                  v
 *                                ERROR
 */
typedef enum {
    SYSTEM_SERVICE_STATE_UNREGISTERED = 0,  /**< Service not registered */
    SYSTEM_SERVICE_STATE_REGISTERED,        /**< Service registered but not started */
    SYSTEM_SERVICE_STATE_RUNNING,           /**< Service actively running */
    SYSTEM_SERVICE_STATE_PAUSED,            /**< Service temporarily paused */
    SYSTEM_SERVICE_STATE_STOPPING,          /**< Service is stopping (cleanup in progress) */
    SYSTEM_SERVICE_STATE_ERROR,             /**< Service encountered error */
} system_service_state_t;

/* ============================================================================
 * Event Priority Enumeration
 * ============================================================================ */

/**
 * @brief Event priority levels
 * 
 * Priority determines processing order:
 * - CRITICAL: Processed immediately (e.g., safety events)
 * - HIGH: Processed before normal events (e.g., user input)
 * - NORMAL: Standard priority (e.g., status updates)
 * - LOW: Processed when queue is not busy (e.g., logging)
 */
typedef enum {
    SYSTEM_EVENT_PRIORITY_LOW = 0,      /**< Low priority, processed last */
    SYSTEM_EVENT_PRIORITY_NORMAL,       /**< Normal priority, default */
    SYSTEM_EVENT_PRIORITY_HIGH,         /**< High priority, processed early */
    SYSTEM_EVENT_PRIORITY_CRITICAL,     /**< Critical priority, processed immediately */
} system_event_priority_t;

/* ============================================================================
 * Event Structure
 * ============================================================================ */

/**
 * @brief System event structure
 * 
 * Events are the primary communication mechanism between services and apps.
 * Data is always copied when posting events to ensure thread safety.
 * 
 * @note The data pointer is managed by the event bus and will be freed
 *       after all handlers have processed the event.
 */
typedef struct {
    system_event_type_t event_type;     /**< Event type identifier */
    system_event_priority_t priority;   /**< Event priority level */
    void *data;                         /**< Event data payload (copied) */
    size_t data_size;                   /**< Size of data payload in bytes */
    uint32_t timestamp;                 /**< Event creation timestamp (ms) */
    system_service_id_t sender_id;      /**< Service that posted the event */
    uint32_t sequence_number;           /**< Event sequence number for ordering */
} system_event_t;

/**
 * @brief Event handler callback function
 * 
 * @param event Pointer to event structure (read-only)
 * @param user_data User data provided during subscription
 * 
 * @warning Handlers should execute quickly (< 50ms recommended).
 *          For long operations, spawn a separate task.
 * @warning Handlers are called from event task context, not the sender's context.
 */
typedef void (*system_event_handler_t)(const system_event_t *event, void *user_data);

/* ============================================================================
 * Service Information Structure
 * ============================================================================ */

/**
 * @brief Service information structure
 * 
 * Contains metadata about a registered service.
 */
typedef struct {
    char name[SYSTEM_SERVICE_MAX_NAME_LEN];  /**< Service name */
    system_service_id_t service_id;          /**< Unique service identifier */
    system_service_state_t state;            /**< Current service state */
    uint32_t last_heartbeat;                 /**< Last heartbeat timestamp (ms) */
    void *service_context;                   /**< Service-specific context pointer */
    bool is_critical;                        /**< Critical service (won't auto-restart) */
    uint32_t restart_count;                  /**< Number of times service was restarted */
} system_service_info_t;

/* ============================================================================
 * Watchdog Configuration
 * ============================================================================ */

/**
 * @brief Service watchdog configuration
 * 
 * Configures watchdog monitoring for a service.
 */
typedef struct {
    uint32_t timeout_ms;            /**< Heartbeat timeout in milliseconds */
    bool auto_restart;              /**< Automatically restart on timeout */
    uint8_t max_restart_attempts;   /**< Maximum restart attempts (0=unlimited) */
    bool is_critical;               /**< Critical service (enter safe mode on failure) */
} service_watchdog_config_t;

/* ============================================================================
 * Service Dependencies
 * ============================================================================ */

/**
 * @brief Service dependency descriptor
 * 
 * Describes dependencies between services for initialization ordering.
 */
typedef struct {
    const char *service_name;                               /**< Name of this service */
    const char *depends_on[SYSTEM_SERVICE_MAX_DEPENDENCIES]; /**< Array of dependency names */
    uint8_t dependency_count;                               /**< Number of dependencies */
} service_dependency_t;

/* ============================================================================
 * Resource Quotas
 * ============================================================================ */

/**
 * @brief Per-service resource quotas
 * 
 * Limits resource usage to prevent misbehaving services from
 * exhausting system resources.
 */
typedef struct {
    uint32_t max_events_per_sec;    /**< Maximum events posted per second */
    uint32_t max_subscriptions;     /**< Maximum event subscriptions */
    uint32_t max_event_data_size;   /**< Maximum event data size in bytes */
    uint32_t max_memory_bytes;      /**< Maximum memory allocation in bytes */
} service_quota_t;

/**
 * @brief Service quota usage tracking
 * 
 * Tracks current resource usage against quotas.
 */
typedef struct {
    uint32_t events_this_sec;       /**< Events posted in current second */
    uint32_t total_events_posted;   /**< Total events posted */
    uint32_t active_subscriptions;  /**< Current active subscriptions */
    uint32_t current_memory_bytes;  /**< Current memory usage */
    uint32_t quota_violations;      /**< Number of quota violations */
} service_quota_usage_t;

/* ============================================================================
 * Performance Metrics
 * ============================================================================ */

/**
 * @brief Per-service performance metrics
 * 
 * Tracks performance statistics for monitoring and debugging.
 */
typedef struct {
    uint64_t total_events_posted;       /**< Total events posted by service */
    uint64_t total_events_received;     /**< Total events received by service */
    uint32_t avg_handler_time_us;       /**< Average handler execution time (μs) */
    uint32_t max_handler_time_us;       /**< Maximum handler execution time (μs) */
    uint32_t handler_timeouts;          /**< Number of handler timeouts */
    uint32_t quota_violations;          /**< Number of quota violations */
    uint64_t total_memory_allocated;    /**< Total memory allocated (bytes) */
    uint32_t last_update_timestamp;     /**< Last metrics update (ms) */
} service_metrics_t;

/**
 * @brief Global system metrics
 * 
 * System-wide performance and health metrics.
 */
typedef struct {
    uint32_t total_services;            /**< Total registered services */
    uint32_t running_services;          /**< Services in RUNNING state */
    uint32_t error_services;            /**< Services in ERROR state */
    uint64_t total_events_processed;    /**< Total events processed */
    uint32_t avg_event_latency_us;      /**< Average event latency (μs) */
    uint32_t max_event_latency_us;      /**< Maximum event latency (μs) */
    uint32_t event_queue_depth;         /**< Current event queue depth */
    uint32_t event_queue_overflows;     /**< Total queue overflow events */
    uint32_t service_restarts;          /**< Total service restarts */
    uint32_t watchdog_timeouts;         /**< Total watchdog timeouts */
    size_t free_heap_bytes;             /**< Free heap memory */
    size_t min_free_heap_bytes;         /**< Minimum free heap ever */
    uint32_t uptime_seconds;            /**< System uptime in seconds */
} global_metrics_t;

/* ============================================================================
 * Event Queue Statistics
 * ============================================================================ */

/**
 * @brief Event queue statistics
 * 
 * Tracks event queue health and performance.
 */
typedef struct {
    uint32_t high_priority_depth;       /**< High priority queue depth */
    uint32_t normal_priority_depth;     /**< Normal priority queue depth */
    uint32_t low_priority_depth;        /**< Low priority queue depth */
    uint32_t high_priority_overflows;   /**< High priority overflows */
    uint32_t normal_priority_overflows; /**< Normal priority overflows */
    uint32_t low_priority_overflows;    /**< Low priority overflows */
    uint32_t low_priority_drops;        /**< Low priority events dropped */
    uint64_t total_events_queued;       /**< Total events ever queued */
    uint64_t total_events_processed;    /**< Total events processed */
} event_queue_stats_t;

/* ============================================================================
 * Event Versioning Support
 * ============================================================================ */

/**
 * @brief Versioned event data header
 * 
 * Prefix event data with version information for backward compatibility.
 * 
 * Usage:
 * @code
 * typedef struct {
 *     versioned_event_header_t header;
 *     uint8_t volume;
 *     bool muted;
 *     // Future fields can be added here
 * } audio_volume_event_v1_t;
 * @endcode
 */
typedef struct {
    uint16_t version;   /**< Event data structure version */
    uint16_t size;      /**< Total size including header */
} versioned_event_header_t;

/**
 * @brief Helper macro to initialize versioned event header
 */
#define VERSIONED_EVENT_INIT(ver, type) \
    { .version = (ver), .size = sizeof(type) }

/* ============================================================================
 * Memory Pool Statistics
 * ============================================================================ */

/**
 * @brief Memory pool statistics
 * 
 * Tracks memory pool usage and efficiency.
 */
typedef struct {
    uint32_t pool_size;         /**< Total blocks in pool */
    uint32_t blocks_used;       /**< Currently allocated blocks */
    uint32_t blocks_free;       /**< Currently free blocks */
    uint32_t total_allocations; /**< Total allocations from pool */
    uint32_t total_frees;       /**< Total frees to pool */
    uint32_t allocation_failures; /**< Failed allocations (pool full) */
    uint32_t high_water_mark;   /**< Maximum blocks used simultaneously */
} memory_pool_stats_t;

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_TYPES_H

