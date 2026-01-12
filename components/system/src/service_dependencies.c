/**
 * @file service_dependencies.c
 * @brief Service dependency management implementation
 */

#include "service_dependencies.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "system_service/error_codes.h"
#include <string.h>

static const char *TAG = "dependencies";

/* ============================================================================
 * Dependency Entry Structure
 * ============================================================================ */

typedef struct {
    char service_name[SYSTEM_SERVICE_MAX_NAME_LEN];
    char depends_on[SYSTEM_SERVICE_MAX_DEPENDENCIES][SYSTEM_SERVICE_MAX_NAME_LEN];
    uint8_t dependency_count;
    bool initialized;
    bool visited;  // For cycle detection
    bool in_stack; // For cycle detection
} dependency_entry_t;

/* ============================================================================
 * Global State
 * ============================================================================ */

typedef struct {
    bool initialized;
    SemaphoreHandle_t mutex;
    dependency_entry_t entries[SYSTEM_SERVICE_MAX_SERVICES];
    uint8_t entry_count;
} dependency_context_t;

static dependency_context_t g_dep_ctx = {0};

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static dependency_entry_t* find_entry(const char *service_name)
{
    for (int i = 0; i < g_dep_ctx.entry_count; i++) {
        if (strcmp(g_dep_ctx.entries[i].service_name, service_name) == 0) {
            return &g_dep_ctx.entries[i];
        }
    }
    return NULL;
}

static dependency_entry_t* create_entry(const char *service_name)
{
    if (g_dep_ctx.entry_count >= SYSTEM_SERVICE_MAX_SERVICES) {
        return NULL;
    }
    
    dependency_entry_t *entry = &g_dep_ctx.entries[g_dep_ctx.entry_count++];
    strncpy(entry->service_name, service_name, SYSTEM_SERVICE_MAX_NAME_LEN - 1);
    entry->service_name[SYSTEM_SERVICE_MAX_NAME_LEN - 1] = '\0';
    entry->dependency_count = 0;
    entry->initialized = false;
    entry->visited = false;
    entry->in_stack = false;
    
    return entry;
}

/**
 * @brief Detect circular dependencies using DFS
 */
static bool has_cycle_dfs(dependency_entry_t *entry)
{
    if (entry->in_stack) {
        return true; // Cycle detected
    }
    
    if (entry->visited) {
        return false; // Already checked
    }
    
    entry->visited = true;
    entry->in_stack = true;
    
    // Check all dependencies
    for (int i = 0; i < entry->dependency_count; i++) {
        dependency_entry_t *dep = find_entry(entry->depends_on[i]);
        if (dep != NULL && has_cycle_dfs(dep)) {
            return true;
        }
    }
    
    entry->in_stack = false;
    return false;
}

/**
 * @brief Topological sort using DFS
 */
static void topo_sort_dfs(dependency_entry_t *entry, const char **order, size_t *index)
{
    if (entry->visited) {
        return;
    }
    
    entry->visited = true;
    
    // Visit dependencies first
    for (int i = 0; i < entry->dependency_count; i++) {
        dependency_entry_t *dep = find_entry(entry->depends_on[i]);
        if (dep != NULL) {
            topo_sort_dfs(dep, order, index);
        }
    }
    
    // Add this service to order
    order[(*index)++] = entry->service_name;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

esp_err_t dependencies_init(void)
{
    if (g_dep_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing dependency system...");
    
    g_dep_ctx.mutex = xSemaphoreCreateMutex();
    if (g_dep_ctx.mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    
    g_dep_ctx.entry_count = 0;
    memset(g_dep_ctx.entries, 0, sizeof(g_dep_ctx.entries));
    
    g_dep_ctx.initialized = true;
    
    ESP_LOGI(TAG, "Dependency system initialized");
    return ESP_OK;
}

esp_err_t dependencies_deinit(void)
{
    if (!g_dep_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_dep_ctx.mutex != NULL) {
        vSemaphoreDelete(g_dep_ctx.mutex);
        g_dep_ctx.mutex = NULL;
    }
    
    g_dep_ctx.initialized = false;
    
    ESP_LOGI(TAG, "Dependency system deinitialized");
    return ESP_OK;
}

esp_err_t dependencies_add(const char *service_name, const char *depends_on)
{
    if (!g_dep_ctx.initialized || service_name == NULL || depends_on == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_dep_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Find or create entry for service
    dependency_entry_t *entry = find_entry(service_name);
    if (entry == NULL) {
        entry = create_entry(service_name);
        if (entry == NULL) {
            xSemaphoreGive(g_dep_ctx.mutex);
            return ESP_ERR_NO_MEM;
        }
    }
    
    // Check if dependency already exists
    for (int i = 0; i < entry->dependency_count; i++) {
        if (strcmp(entry->depends_on[i], depends_on) == 0) {
            xSemaphoreGive(g_dep_ctx.mutex);
            return ESP_OK; // Already added
        }
    }
    
    // Add dependency
    if (entry->dependency_count >= SYSTEM_SERVICE_MAX_DEPENDENCIES) {
        xSemaphoreGive(g_dep_ctx.mutex);
        return ESP_ERR_NO_MEM;
    }
    
    strncpy(entry->depends_on[entry->dependency_count], depends_on, SYSTEM_SERVICE_MAX_NAME_LEN - 1);
    entry->depends_on[entry->dependency_count][SYSTEM_SERVICE_MAX_NAME_LEN - 1] = '\0';
    entry->dependency_count++;
    
    // Ensure dependency service exists in graph
    if (find_entry(depends_on) == NULL) {
        create_entry(depends_on);
    }
    
    // Check for circular dependencies
    // Reset visited flags
    for (int i = 0; i < g_dep_ctx.entry_count; i++) {
        g_dep_ctx.entries[i].visited = false;
        g_dep_ctx.entries[i].in_stack = false;
    }
    
    if (has_cycle_dfs(entry)) {
        // Remove the dependency we just added
        entry->dependency_count--;
        xSemaphoreGive(g_dep_ctx.mutex);
        ESP_LOGE(TAG, "Circular dependency detected: %s -> %s", service_name, depends_on);
        return ESP_ERR_SERVICE_CIRCULAR_DEPENDENCY;
    }
    
    xSemaphoreGive(g_dep_ctx.mutex);
    
    ESP_LOGI(TAG, "Added dependency: %s depends on %s", service_name, depends_on);
    return ESP_OK;
}

esp_err_t dependencies_add_multiple(const service_dependency_t *dependency)
{
    if (dependency == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (int i = 0; i < dependency->dependency_count; i++) {
        esp_err_t ret = dependencies_add(dependency->service_name, dependency->depends_on[i]);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    
    return ESP_OK;
}

esp_err_t dependencies_get_init_order(const char **order, size_t max_count, size_t *out_count)
{
    if (!g_dep_ctx.initialized || order == NULL || out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_dep_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Reset visited flags
    for (int i = 0; i < g_dep_ctx.entry_count; i++) {
        g_dep_ctx.entries[i].visited = false;
    }
    
    // Perform topological sort
    size_t index = 0;
    for (int i = 0; i < g_dep_ctx.entry_count; i++) {
        if (!g_dep_ctx.entries[i].visited) {
            topo_sort_dfs(&g_dep_ctx.entries[i], order, &index);
        }
    }
    
    *out_count = index;
    
    xSemaphoreGive(g_dep_ctx.mutex);
    
    return ESP_OK;
}

esp_err_t dependencies_check_ready(const char *service_name)
{
    if (!g_dep_ctx.initialized || service_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_dep_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    dependency_entry_t *entry = find_entry(service_name);
    if (entry == NULL) {
        xSemaphoreGive(g_dep_ctx.mutex);
        return ESP_OK; // No dependencies
    }
    
    // Check if all dependencies are initialized
    for (int i = 0; i < entry->dependency_count; i++) {
        dependency_entry_t *dep = find_entry(entry->depends_on[i]);
        if (dep == NULL || !dep->initialized) {
            xSemaphoreGive(g_dep_ctx.mutex);
            ESP_LOGW(TAG, "Service %s waiting for dependency: %s", 
                     service_name, entry->depends_on[i]);
            return ESP_ERR_SERVICE_DEPENDENCY_FAILED;
        }
    }
    
    xSemaphoreGive(g_dep_ctx.mutex);
    return ESP_OK;
}

esp_err_t dependencies_mark_initialized(const char *service_name)
{
    if (!g_dep_ctx.initialized || service_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_dep_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    dependency_entry_t *entry = find_entry(service_name);
    if (entry != NULL) {
        entry->initialized = true;
        ESP_LOGI(TAG, "Service %s marked as initialized", service_name);
    }
    
    xSemaphoreGive(g_dep_ctx.mutex);
    return ESP_OK;
}

void dependencies_log_graph(const char *tag)
{
    if (!g_dep_ctx.initialized) {
        ESP_LOGW(tag, "Dependency system not initialized");
        return;
    }
    
    ESP_LOGI(tag, "Service Dependency Graph:");
    
    for (int i = 0; i < g_dep_ctx.entry_count; i++) {
        dependency_entry_t *entry = &g_dep_ctx.entries[i];
        
        if (entry->dependency_count == 0) {
            ESP_LOGI(tag, "  %s (no dependencies) [%s]",
                     entry->service_name,
                     entry->initialized ? "INIT" : "NOT INIT");
        } else {
            ESP_LOGI(tag, "  %s depends on: [%s]",
                     entry->service_name,
                     entry->initialized ? "INIT" : "NOT INIT");
            for (int j = 0; j < entry->dependency_count; j++) {
                ESP_LOGI(tag, "    - %s", entry->depends_on[j]);
            }
        }
    }
    
    // Show initialization order
    const char *order[SYSTEM_SERVICE_MAX_SERVICES];
    size_t count;
    if (dependencies_get_init_order(order, SYSTEM_SERVICE_MAX_SERVICES, &count) == ESP_OK) {
        ESP_LOGI(tag, "Recommended initialization order:");
        for (size_t i = 0; i < count; i++) {
            ESP_LOGI(tag, "  %zu. %s", i + 1, order[i]);
        }
    }
}
