/**
 * @file log_control.c
 * @brief Per-service log level control implementation
 */

#include "log_control.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "system_service/service_manager.h"
#include <string.h>

static const char *TAG = "log_control";

/* ============================================================================
 * Log Level Tracking
 * ============================================================================ */

typedef struct {
    system_service_id_t service_id;
    char service_name[SYSTEM_SERVICE_MAX_NAME_LEN];
    esp_log_level_t level;
    bool active;
} service_log_config_t;

static service_log_config_t g_log_configs[SYSTEM_SERVICE_MAX_SERVICES] = {0};
static SemaphoreHandle_t g_log_mutex = NULL;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static void ensure_mutex(void)
{
    if (g_log_mutex == NULL) {
        g_log_mutex = xSemaphoreCreateMutex();
    }
}

static service_log_config_t* find_config(system_service_id_t service_id)
{
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (g_log_configs[i].active && g_log_configs[i].service_id == service_id) {
            return &g_log_configs[i];
        }
    }
    return NULL;
}

static service_log_config_t* find_config_by_name(const char *service_name)
{
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (g_log_configs[i].active && 
            strcmp(g_log_configs[i].service_name, service_name) == 0) {
            return &g_log_configs[i];
        }
    }
    return NULL;
}

static service_log_config_t* get_or_create_config(system_service_id_t service_id)
{
    service_log_config_t *config = find_config(service_id);
    if (config != NULL) {
        return config;
    }
    
    // Find empty slot
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (!g_log_configs[i].active) {
            g_log_configs[i].active = true;
            g_log_configs[i].service_id = service_id;
            g_log_configs[i].level = ESP_LOG_INFO; // Default
            
            // Try to get service name
            system_service_info_t info;
            if (system_service_get_info(service_id, &info) == ESP_OK) {
                strncpy(g_log_configs[i].service_name, info.name, 
                        SYSTEM_SERVICE_MAX_NAME_LEN - 1);
                g_log_configs[i].service_name[SYSTEM_SERVICE_MAX_NAME_LEN - 1] = '\0';
            }
            
            return &g_log_configs[i];
        }
    }
    
    return NULL;
}

static const char* log_level_to_string(esp_log_level_t level)
{
    switch (level) {
        case ESP_LOG_NONE: return "NONE";
        case ESP_LOG_ERROR: return "ERROR";
        case ESP_LOG_WARN: return "WARN";
        case ESP_LOG_INFO: return "INFO";
        case ESP_LOG_DEBUG: return "DEBUG";
        case ESP_LOG_VERBOSE: return "VERBOSE";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

esp_err_t log_control_set_level(system_service_id_t service_id, esp_log_level_t level)
{
    if (level < ESP_LOG_NONE || level > ESP_LOG_VERBOSE) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ensure_mutex();
    
    if (xSemaphoreTake(g_log_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    service_log_config_t *config = get_or_create_config(service_id);
    if (config == NULL) {
        xSemaphoreGive(g_log_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    config->level = level;
    
    // Set ESP-IDF log level for service tag
    if (config->service_name[0] != '\0') {
        esp_log_level_set(config->service_name, level);
    }
    
    xSemaphoreGive(g_log_mutex);
    
    ESP_LOGI(TAG, "Set log level for service %d to %s", 
             service_id, log_level_to_string(level));
    
    return ESP_OK;
}

esp_err_t log_control_get_level(system_service_id_t service_id, esp_log_level_t *level)
{
    if (level == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ensure_mutex();
    
    if (xSemaphoreTake(g_log_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    service_log_config_t *config = find_config(service_id);
    if (config == NULL) {
        *level = ESP_LOG_INFO; // Default
    } else {
        *level = config->level;
    }
    
    xSemaphoreGive(g_log_mutex);
    
    return ESP_OK;
}

esp_err_t log_control_set_level_by_name(const char *service_name, esp_log_level_t level)
{
    if (service_name == NULL || level < ESP_LOG_NONE || level > ESP_LOG_VERBOSE) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ensure_mutex();
    
    if (xSemaphoreTake(g_log_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    service_log_config_t *config = find_config_by_name(service_name);
    if (config != NULL) {
        config->level = level;
    }
    
    // Set ESP-IDF log level
    esp_log_level_set(service_name, level);
    
    xSemaphoreGive(g_log_mutex);
    
    ESP_LOGI(TAG, "Set log level for service '%s' to %s", 
             service_name, log_level_to_string(level));
    
    return ESP_OK;
}

esp_err_t log_control_reset_all(void)
{
    ensure_mutex();
    
    if (xSemaphoreTake(g_log_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (g_log_configs[i].active) {
            g_log_configs[i].level = ESP_LOG_INFO;
            if (g_log_configs[i].service_name[0] != '\0') {
                esp_log_level_set(g_log_configs[i].service_name, ESP_LOG_INFO);
            }
        }
    }
    
    xSemaphoreGive(g_log_mutex);
    
    ESP_LOGI(TAG, "Reset all service log levels to INFO");
    return ESP_OK;
}

void log_control_log_status(const char *tag)
{
    ensure_mutex();
    
    if (xSemaphoreTake(g_log_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(tag, "Failed to acquire log mutex");
        return;
    }
    
    ESP_LOGI(tag, "Service Log Levels:");
    
    for (int i = 0; i < SYSTEM_SERVICE_MAX_SERVICES; i++) {
        if (g_log_configs[i].active) {
            ESP_LOGI(tag, "  %s (ID %d): %s",
                     g_log_configs[i].service_name,
                     g_log_configs[i].service_id,
                     log_level_to_string(g_log_configs[i].level));
        }
    }
    
    xSemaphoreGive(g_log_mutex);
}
