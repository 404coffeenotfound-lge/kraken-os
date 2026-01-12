/**
 * @file service_watchdog.c
 * @brief Service watchdog implementation with automatic recovery
 */

#include "service_watchdog.h"
#include "system_internal.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "system_service/error_codes.h"
#include "system_service/service_manager.h"
#include <string.h>

static const char *TAG = "watchdog";

/* ============================================================================
 * Watchdog Entry Structure
 * ============================================================================ */

typedef struct {
    bool active;                        /**< Entry is active */
    system_service_id_t service_id;     /**< Service being monitored */
    service_watchdog_config_t config;   /**< Watchdog configuration */
    uint32_t last_heartbeat;            /**< Last heartbeat timestamp */
    uint8_t restart_attempts;           /**< Current restart attempts */
    bool timeout_detected;              /**< Timeout currently detected */
} watchdog_entry_t;

/* ============================================================================
 * Global State
 * ============================================================================ */

typedef struct {
    bool initialized;                   /**< Watchdog initialized */
    bool running;                       /**< Watchdog task running */
    TaskHandle_t task_handle;           /**< Watchdog task handle */
    SemaphoreHandle_t mutex;            /**< Mutex for thread safety */
    watchdog_entry_t entries[SYSTEM_SERVICE_MAX_SERVICES];
    watchdog_stats_t stats;             /**< Global statistics */
    bool safe_mode;                     /**< System in safe mode */
} watchdog_context_t;

static watchdog_context_t g_watchdog_ctx = {0};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void watchdog_task(void *arg);
static esp_err_t restart_service(system_service_id_t service_id);
static void enter_safe_mode(const char *reason);

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static inline uint32_t get_time_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static watchdog_entry_t* find_entry(system_service_id_t service_id)
{
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (g_watchdog_ctx.entries[i].active && 
            g_watchdog_ctx.entries[i].service_id == service_id) {
            return &g_watchdog_ctx.entries[i];
        }
    }
    return NULL;
}

static watchdog_entry_t* allocate_entry(void)
{
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (!g_watchdog_ctx.entries[i].active) {
            return &g_watchdog_ctx.entries[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Service Restart Logic
 * ============================================================================ */

static esp_err_t restart_service(system_service_id_t service_id)
{
    ESP_LOGW(TAG, "Attempting to restart service %d", service_id);
    
    // Get service info
    system_service_info_t info;
    esp_err_t ret = system_service_get_info(service_id, &info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get service info: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Restarting service: %s", info.name);
    
    // TODO: Implement actual service restart logic
    // This requires integration with service_manager to:
    // 1. Stop the service
    // 2. Clean up resources
    // 3. Reinitialize the service
    // 4. Restart the service
    
    // For now, just set state to ERROR
    ret = system_service_set_state(service_id, SYSTEM_SERVICE_STATE_ERROR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set service state: %s", esp_err_to_name(ret));
        return ESP_ERR_SERVICE_RESTART_FAILED;
    }
    
    ESP_LOGI(TAG, "Service %s marked for restart", info.name);
    return ESP_OK;
}

static void enter_safe_mode(const char *reason)
{
    if (g_watchdog_ctx.safe_mode) {
        return; // Already in safe mode
    }
    
    ESP_LOGE(TAG, "═══════════════════════════════════════════════════");
    ESP_LOGE(TAG, "ENTERING SAFE MODE");
    ESP_LOGE(TAG, "Reason: %s", reason);
    ESP_LOGE(TAG, "═══════════════════════════════════════════════════");
    
    g_watchdog_ctx.safe_mode = true;
    g_watchdog_ctx.stats.safe_mode_active = true;
    g_watchdog_ctx.stats.critical_failures++;
    
    // TODO: Implement safe mode actions:
    // - Stop non-critical services
    // - Disable event processing
    // - Enter minimal operation mode
    // - Log diagnostic information
}

/* ============================================================================
 * Watchdog Task
 * ============================================================================ */

static void watchdog_task(void *arg)
{
    ESP_LOGI(TAG, "Watchdog task started");
    
    const uint32_t check_interval_ms = CONFIG_SYSTEM_SERVICE_WATCHDOG_CHECK_INTERVAL_MS;
    
    while (g_watchdog_ctx.running) {
        uint32_t now = get_time_ms();
        
        // Lock mutex
        if (xSemaphoreTake(g_watchdog_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to acquire watchdog mutex");
            vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
            continue;
        }
        
        // Check all monitored services
        for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
            watchdog_entry_t *entry = &g_watchdog_ctx.entries[i];
            
            if (!entry->active) {
                continue;
            }
            
            // Calculate time since last heartbeat
            uint32_t elapsed = now - entry->last_heartbeat;
            
            // Check for timeout
            if (elapsed > entry->config.timeout_ms) {
                if (!entry->timeout_detected) {
                    // First timeout detection
                    entry->timeout_detected = true;
                    g_watchdog_ctx.stats.total_timeouts++;
                    
                    ESP_LOGW(TAG, "Service %d timeout detected (elapsed=%lu ms, timeout=%lu ms)",
                             entry->service_id, elapsed, entry->config.timeout_ms);
                    
                    // Handle timeout based on configuration
                    if (entry->config.is_critical) {
                        // Critical service - enter safe mode
                        char reason[64];
                        snprintf(reason, sizeof(reason), "Critical service %d timeout", entry->service_id);
                        enter_safe_mode(reason);
                    } else if (entry->config.auto_restart) {
                        // Check restart attempts
                        if (entry->config.max_restart_attempts == 0 || 
                            entry->restart_attempts < entry->config.max_restart_attempts) {
                            
                            // Attempt restart
                            entry->restart_attempts++;
                            g_watchdog_ctx.stats.total_restarts++;
                            
                            esp_err_t ret = restart_service(entry->service_id);
                            if (ret != ESP_OK) {
                                ESP_LOGE(TAG, "Service %d restart failed (attempt %d)",
                                         entry->service_id, entry->restart_attempts);
                                g_watchdog_ctx.stats.failed_restarts++;
                                
                                // Check if max attempts reached
                                if (entry->config.max_restart_attempts > 0 &&
                                    entry->restart_attempts >= entry->config.max_restart_attempts) {
                                    ESP_LOGE(TAG, "Service %d exceeded max restart attempts",
                                             entry->service_id);
                                    // Mark as critical failure
                                    g_watchdog_ctx.stats.critical_failures++;
                                }
                            } else {
                                ESP_LOGI(TAG, "Service %d restart initiated (attempt %d)",
                                         entry->service_id, entry->restart_attempts);
                                // Reset heartbeat timestamp
                                entry->last_heartbeat = now;
                                entry->timeout_detected = false;
                            }
                        } else {
                            ESP_LOGE(TAG, "Service %d exceeded max restart attempts (%d)",
                                     entry->service_id, entry->config.max_restart_attempts);
                        }
                    } else {
                        ESP_LOGW(TAG, "Service %d timeout (auto-restart disabled)", entry->service_id);
                    }
                }
            } else {
                // Service is healthy
                if (entry->timeout_detected) {
                    ESP_LOGI(TAG, "Service %d recovered", entry->service_id);
                    entry->timeout_detected = false;
                    entry->restart_attempts = 0;
                }
            }
        }
        
        xSemaphoreGive(g_watchdog_ctx.mutex);
        
        // Sleep until next check
        vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
    }
    
    ESP_LOGI(TAG, "Watchdog task stopped");
    vTaskDelete(NULL);
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

esp_err_t watchdog_init(void)
{
    if (g_watchdog_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing watchdog system...");
    
    // Create mutex
    g_watchdog_ctx.mutex = xSemaphoreCreateMutex();
    if (g_watchdog_ctx.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create watchdog mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize entries
    memset(g_watchdog_ctx.entries, 0, sizeof(g_watchdog_ctx.entries));
    memset(&g_watchdog_ctx.stats, 0, sizeof(watchdog_stats_t));
    
    g_watchdog_ctx.initialized = true;
    g_watchdog_ctx.running = false;
    g_watchdog_ctx.safe_mode = false;
    
    ESP_LOGI(TAG, "Watchdog system initialized");
    return ESP_OK;
}

esp_err_t watchdog_start(void)
{
    if (!g_watchdog_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_watchdog_ctx.running) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting watchdog monitoring...");
    
    g_watchdog_ctx.running = true;
    
    // Create watchdog task
    BaseType_t ret = xTaskCreate(
        watchdog_task,
        "watchdog",
        4096,
        NULL,
        CONFIG_SYSTEM_SERVICE_WATCHDOG_TASK_PRIORITY,
        &g_watchdog_ctx.task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create watchdog task");
        g_watchdog_ctx.running = false;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Watchdog monitoring started");
    return ESP_OK;
}

esp_err_t watchdog_stop(void)
{
    if (!g_watchdog_ctx.initialized || !g_watchdog_ctx.running) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Stopping watchdog monitoring...");
    
    g_watchdog_ctx.running = false;
    
    // Wait for task to exit
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "Watchdog monitoring stopped");
    return ESP_OK;
}

esp_err_t watchdog_deinit(void)
{
    if (!g_watchdog_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_watchdog_ctx.running) {
        watchdog_stop();
    }
    
    ESP_LOGI(TAG, "Deinitializing watchdog system...");
    
    if (g_watchdog_ctx.mutex != NULL) {
        vSemaphoreDelete(g_watchdog_ctx.mutex);
        g_watchdog_ctx.mutex = NULL;
    }
    
    g_watchdog_ctx.initialized = false;
    
    ESP_LOGI(TAG, "Watchdog system deinitialized");
    return ESP_OK;
}

esp_err_t watchdog_register_service(system_service_id_t service_id, 
                                    const service_watchdog_config_t *config)
{
    if (!g_watchdog_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_watchdog_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Check if already registered
    if (find_entry(service_id) != NULL) {
        xSemaphoreGive(g_watchdog_ctx.mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Allocate entry
    watchdog_entry_t *entry = allocate_entry();
    if (entry == NULL) {
        xSemaphoreGive(g_watchdog_ctx.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize entry
    entry->active = true;
    entry->service_id = service_id;
    entry->last_heartbeat = get_time_ms();
    entry->restart_attempts = 0;
    entry->timeout_detected = false;
    
    // Set configuration (use defaults if NULL)
    if (config != NULL) {
        entry->config = *config;
    } else {
        entry->config.timeout_ms = CONFIG_SYSTEM_SERVICE_HEARTBEAT_TIMEOUT_MS;
        entry->config.auto_restart = CONFIG_SYSTEM_SERVICE_WATCHDOG_AUTO_RESTART;
        entry->config.max_restart_attempts = CONFIG_SYSTEM_SERVICE_WATCHDOG_MAX_RESTARTS;
        entry->config.is_critical = false;
    }
    
    xSemaphoreGive(g_watchdog_ctx.mutex);
    
    ESP_LOGI(TAG, "Service %d registered with watchdog (timeout=%lu ms, auto_restart=%d)",
             service_id, entry->config.timeout_ms, entry->config.auto_restart);
    
    return ESP_OK;
}

esp_err_t watchdog_unregister_service(system_service_id_t service_id)
{
    if (!g_watchdog_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_watchdog_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    watchdog_entry_t *entry = find_entry(service_id);
    if (entry == NULL) {
        xSemaphoreGive(g_watchdog_ctx.mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    entry->active = false;
    
    xSemaphoreGive(g_watchdog_ctx.mutex);
    
    ESP_LOGI(TAG, "Service %d unregistered from watchdog", service_id);
    return ESP_OK;
}

esp_err_t watchdog_update_heartbeat(system_service_id_t service_id, uint32_t timestamp)
{
    if (!g_watchdog_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_watchdog_ctx.mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        // Don't block on heartbeat updates
        return ESP_ERR_TIMEOUT;
    }
    
    watchdog_entry_t *entry = find_entry(service_id);
    if (entry != NULL) {
        entry->last_heartbeat = timestamp;
    }
    
    xSemaphoreGive(g_watchdog_ctx.mutex);
    
    return ESP_OK;
}

esp_err_t watchdog_enable_service(system_service_id_t service_id)
{
    if (!g_watchdog_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_watchdog_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    watchdog_entry_t *entry = find_entry(service_id);
    if (entry == NULL) {
        xSemaphoreGive(g_watchdog_ctx.mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    entry->active = true;
    entry->last_heartbeat = get_time_ms();
    
    xSemaphoreGive(g_watchdog_ctx.mutex);
    
    return ESP_OK;
}

esp_err_t watchdog_disable_service(system_service_id_t service_id)
{
    if (!g_watchdog_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_watchdog_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    watchdog_entry_t *entry = find_entry(service_id);
    if (entry == NULL) {
        xSemaphoreGive(g_watchdog_ctx.mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    entry->active = false;
    
    xSemaphoreGive(g_watchdog_ctx.mutex);
    
    return ESP_OK;
}

esp_err_t watchdog_reset_restart_count(system_service_id_t service_id)
{
    if (!g_watchdog_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_watchdog_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    watchdog_entry_t *entry = find_entry(service_id);
    if (entry == NULL) {
        xSemaphoreGive(g_watchdog_ctx.mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    entry->restart_attempts = 0;
    
    xSemaphoreGive(g_watchdog_ctx.mutex);
    
    return ESP_OK;
}

esp_err_t watchdog_get_stats(watchdog_stats_t *stats)
{
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_watchdog_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_watchdog_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memcpy(stats, &g_watchdog_ctx.stats, sizeof(watchdog_stats_t));
    
    xSemaphoreGive(g_watchdog_ctx.mutex);
    
    return ESP_OK;
}

void watchdog_log_status(const char *tag)
{
    if (!g_watchdog_ctx.initialized) {
        ESP_LOGW(tag, "Watchdog not initialized");
        return;
    }
    
    ESP_LOGI(tag, "Watchdog Status:");
    ESP_LOGI(tag, "  Running: %s", g_watchdog_ctx.running ? "YES" : "NO");
    ESP_LOGI(tag, "  Safe Mode: %s", g_watchdog_ctx.safe_mode ? "YES" : "NO");
    ESP_LOGI(tag, "  Total Timeouts: %lu", g_watchdog_ctx.stats.total_timeouts);
    ESP_LOGI(tag, "  Total Restarts: %lu", g_watchdog_ctx.stats.total_restarts);
    ESP_LOGI(tag, "  Failed Restarts: %lu", g_watchdog_ctx.stats.failed_restarts);
    ESP_LOGI(tag, "  Critical Failures: %lu", g_watchdog_ctx.stats.critical_failures);
    
    // Count monitored services
    uint32_t monitored = 0;
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (g_watchdog_ctx.entries[i].active) {
            monitored++;
        }
    }
    ESP_LOGI(tag, "  Monitored Services: %lu", monitored);
}
