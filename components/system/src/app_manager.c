#include "system_service/app_manager.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "app_internal.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "app_manager";
static app_registry_t g_app_registry = {0};

app_registry_t* app_get_registry(void)
{
    return &g_app_registry;
}

esp_err_t app_registry_lock(void)
{
    if (!g_app_registry.initialized || g_app_registry.mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_app_registry.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    return ESP_OK;
}

esp_err_t app_registry_unlock(void)
{
    if (!g_app_registry.initialized || g_app_registry.mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreGive(g_app_registry.mutex);
    return ESP_OK;
}

app_registry_entry_t* app_find_by_name(const char *name)
{
    if (name == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < APP_REGISTRY_MAX_ENTRIES; i++) {
        if (g_app_registry.entries[i].registered &&
            strcmp(g_app_registry.entries[i].info.manifest.name, name) == 0) {
            return &g_app_registry.entries[i];
        }
    }
    
    return NULL;
}

static void app_task_wrapper(void *pvParameters)
{
    app_registry_entry_t *entry = (app_registry_entry_t *)pvParameters;
    
    if (entry == NULL || entry->info.manifest.entry == NULL) {
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Starting app '%s'...", entry->info.manifest.name);
    
    esp_err_t ret = entry->info.manifest.entry(&entry->context);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "App '%s' entry failed: %s", 
                 entry->info.manifest.name, esp_err_to_name(ret));
        entry->info.state = APP_STATE_ERROR;
    }
    
    ESP_LOGI(TAG, "App '%s' task finished", entry->info.manifest.name);
    
    entry->task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t app_manager_init(void)
{
    if (g_app_registry.initialized) {
        ESP_LOGW(TAG, "App manager already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Initializing app manager...");
    
    memset(&g_app_registry, 0, sizeof(app_registry_t));
    
    g_app_registry.mutex = xSemaphoreCreateMutex();
    if (g_app_registry.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    g_app_registry.initialized = true;
    
    ESP_LOGI(TAG, "✓ App manager initialized");
    
    return ESP_OK;
}

esp_err_t app_manager_register_app(const app_manifest_t *manifest, app_info_t **out_info)
{
    if (manifest == NULL || manifest->name[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = app_registry_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Check if app already registered
    if (app_find_by_name(manifest->name) != NULL) {
        ESP_LOGE(TAG, "App '%s' already registered", manifest->name);
        app_registry_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    
    // Find free slot
    int slot = -1;
    for (int i = 0; i < APP_REGISTRY_MAX_ENTRIES; i++) {
        if (!g_app_registry.entries[i].registered) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        ESP_LOGE(TAG, "Maximum apps reached");
        app_registry_unlock();
        return ESP_ERR_NO_MEM;
    }
    
    app_registry_entry_t *entry = &g_app_registry.entries[slot];
    
    // Copy manifest
    memcpy(&entry->info.manifest, manifest, sizeof(app_manifest_t));
    
    // Set app info
    entry->info.state = APP_STATE_LOADED;
    entry->info.source = APP_SOURCE_INTERNAL;
    entry->info.is_dynamic = false;
    entry->info.load_time = (uint32_t)(esp_timer_get_time() / 1000000);
    
    // Setup context with FULL system service API access
    // Apps are first-class citizens with same rights as services
    entry->context.app_info = &entry->info;
    entry->context.service_id = entry->info.service_id;
    
    // Service management APIs
    entry->context.register_service = system_service_register;
    entry->context.unregister_service = system_service_unregister;
    entry->context.set_state = system_service_set_state;
    entry->context.heartbeat = system_service_heartbeat;
    
    // Event bus APIs - full access
    entry->context.post_event = system_event_post;
    entry->context.subscribe_event = system_event_subscribe;
    entry->context.unsubscribe_event = system_event_unsubscribe;
    entry->context.register_event_type = system_event_register_type;
    
    // Register with system service (apps are visible to system)
    ret = system_service_register(manifest->name, entry, &entry->info.service_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register app '%s' with system", manifest->name);
        app_registry_unlock();
        return ret;
    }
    
    // Update context with actual service ID
    entry->context.service_id = entry->info.service_id;
    entry->registered = true;
    g_app_registry.count++;
    
    if (out_info != NULL) {
        *out_info = &entry->info;
    }
    
    app_registry_unlock();
    
    ESP_LOGI(TAG, "✓ Registered app '%s' v%s by %s",
             manifest->name, manifest->version, manifest->author);
    
    return ESP_OK;
}

esp_err_t app_manager_start_app(const char *app_name)
{
    if (app_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = app_registry_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    app_registry_entry_t *entry = app_find_by_name(app_name);
    if (entry == NULL) {
        ESP_LOGE(TAG, "App '%s' not found", app_name);
        app_registry_unlock();
        return ESP_ERR_NOT_FOUND;
    }
    
    if (entry->info.state == APP_STATE_RUNNING) {
        ESP_LOGW(TAG, "App '%s' already running", app_name);
        app_registry_unlock();
        return ESP_OK;
    }
    
    if (entry->info.manifest.entry == NULL) {
        ESP_LOGE(TAG, "App '%s' has no entry function", app_name);
        app_registry_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    
    // Create task for the app
    BaseType_t task_ret = xTaskCreate(
        app_task_wrapper,
        app_name,
        4096,  // Stack size
        entry,
        5,     // Priority
        &entry->task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task for app '%s'", app_name);
        app_registry_unlock();
        return ESP_ERR_NO_MEM;
    }
    
    entry->info.state = APP_STATE_RUNNING;
    
    system_service_set_state(entry->info.service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    app_registry_unlock();
    
    ESP_LOGI(TAG, "✓ Started app '%s'", app_name);
    
    return ESP_OK;
}

esp_err_t app_manager_stop_app(const char *app_name)
{
    if (app_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = app_registry_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    app_registry_entry_t *entry = app_find_by_name(app_name);
    if (entry == NULL) {
        ESP_LOGE(TAG, "App '%s' not found", app_name);
        app_registry_unlock();
        return ESP_ERR_NOT_FOUND;
    }
    
    if (entry->info.state != APP_STATE_RUNNING && entry->info.state != APP_STATE_PAUSED) {
        ESP_LOGW(TAG, "App '%s' not running", app_name);
        app_registry_unlock();
        return ESP_OK;
    }
    
    // Call exit function if available
    if (entry->info.manifest.exit != NULL) {
        entry->info.manifest.exit(&entry->context);
    }
    
    // Delete task if still running
    if (entry->task_handle != NULL) {
        vTaskDelete(entry->task_handle);
        entry->task_handle = NULL;
    }
    
    entry->info.state = APP_STATE_LOADED;
    
    system_service_set_state(entry->info.service_id, SYSTEM_SERVICE_STATE_REGISTERED);
    
    app_registry_unlock();
    
    ESP_LOGI(TAG, "✓ Stopped app '%s'", app_name);
    
    return ESP_OK;
}

esp_err_t app_manager_pause_app(const char *app_name)
{
    if (app_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = app_registry_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    app_registry_entry_t *entry = app_find_by_name(app_name);
    if (entry == NULL) {
        app_registry_unlock();
        return ESP_ERR_NOT_FOUND;
    }
    
    if (entry->info.state != APP_STATE_RUNNING) {
        app_registry_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    
    if (entry->task_handle != NULL) {
        vTaskSuspend(entry->task_handle);
    }
    
    entry->info.state = APP_STATE_PAUSED;
    system_service_set_state(entry->info.service_id, SYSTEM_SERVICE_STATE_PAUSED);
    
    app_registry_unlock();
    
    ESP_LOGI(TAG, "✓ Paused app '%s'", app_name);
    
    return ESP_OK;
}

esp_err_t app_manager_resume_app(const char *app_name)
{
    if (app_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = app_registry_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    app_registry_entry_t *entry = app_find_by_name(app_name);
    if (entry == NULL) {
        app_registry_unlock();
        return ESP_ERR_NOT_FOUND;
    }
    
    if (entry->info.state != APP_STATE_PAUSED) {
        app_registry_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    
    if (entry->task_handle != NULL) {
        vTaskResume(entry->task_handle);
    }
    
    entry->info.state = APP_STATE_RUNNING;
    system_service_set_state(entry->info.service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    app_registry_unlock();
    
    ESP_LOGI(TAG, "✓ Resumed app '%s'", app_name);
    
    return ESP_OK;
}

esp_err_t app_manager_uninstall(const char *app_name)
{
    if (app_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = app_registry_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    app_registry_entry_t *entry = app_find_by_name(app_name);
    if (entry == NULL) {
        app_registry_unlock();
        return ESP_ERR_NOT_FOUND;
    }
    
    // Stop if running
    if (entry->info.state == APP_STATE_RUNNING || entry->info.state == APP_STATE_PAUSED) {
        app_registry_unlock();
        app_manager_stop_app(app_name);
        ret = app_registry_lock();
        if (ret != ESP_OK) {
            return ret;
        }
    }
    
    // Unregister from system
    system_service_unregister(entry->info.service_id);
    
    // Free dynamic memory if allocated
    if (entry->info.is_dynamic && entry->info.app_data != NULL) {
        free(entry->info.app_data);
    }
    
    // Clear entry
    memset(entry, 0, sizeof(app_registry_entry_t));
    g_app_registry.count--;
    
    app_registry_unlock();
    
    ESP_LOGI(TAG, "✓ Uninstalled app '%s'", app_name);
    
    return ESP_OK;
}

esp_err_t app_manager_get_info(const char *app_name, app_info_t *out_info)
{
    if (app_name == NULL || out_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = app_registry_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    app_registry_entry_t *entry = app_find_by_name(app_name);
    if (entry == NULL) {
        app_registry_unlock();
        return ESP_ERR_NOT_FOUND;
    }
    
    memcpy(out_info, &entry->info, sizeof(app_info_t));
    
    app_registry_unlock();
    
    return ESP_OK;
}

esp_err_t app_manager_list_apps(app_info_t *apps, size_t max_count, size_t *out_count)
{
    if (apps == NULL || out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = app_registry_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    size_t count = 0;
    for (int i = 0; i < APP_REGISTRY_MAX_ENTRIES && count < max_count; i++) {
        if (g_app_registry.entries[i].registered) {
            memcpy(&apps[count], &g_app_registry.entries[i].info, sizeof(app_info_t));
            count++;
        }
    }
    
    *out_count = count;
    
    app_registry_unlock();
    
    return ESP_OK;
}

esp_err_t app_manager_get_running_apps(app_info_t *apps, size_t max_count, size_t *out_count)
{
    if (apps == NULL || out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = app_registry_lock();
    if (ret != ESP_OK) {
        return ret;
    }
    
    size_t count = 0;
    for (int i = 0; i < APP_REGISTRY_MAX_ENTRIES && count < max_count; i++) {
        if (g_app_registry.entries[i].registered &&
            g_app_registry.entries[i].info.state == APP_STATE_RUNNING) {
            memcpy(&apps[count], &g_app_registry.entries[i].info, sizeof(app_info_t));
            count++;
        }
    }
    
    *out_count = count;
    
    app_registry_unlock();
    
    return ESP_OK;
}

// Placeholder for future implementation
esp_err_t app_manager_load_from_storage(const char *path, app_info_t **out_info)
{
    ESP_LOGI(TAG, "Load from storage: %s (not yet implemented)", path);
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t app_manager_load_from_url(const char *url, app_info_t **out_info)
{
    ESP_LOGI(TAG, "Load from URL: %s (not yet implemented)", url);
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t app_manager_install(const void *app_data, size_t size, 
                              const char *install_path, app_info_t **out_info)
{
    ESP_LOGI(TAG, "Install to: %s (not yet implemented)", install_path);
    return ESP_ERR_NOT_SUPPORTED;
}
