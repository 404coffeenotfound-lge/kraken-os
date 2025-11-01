#include "system_service/app_manager.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "system_service/app_storage.h"
#include "app_internal.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_partition.h"
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
    if (path == NULL || out_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Loading app from storage: %s", path);
    
    // Extract app name from path (e.g., "/storage/apps/goodbye.bin" -> "goodbye")
    const char *filename = strrchr(path, '/');
    if (filename == NULL) {
        filename = path;
    } else {
        filename++; // Skip the '/'
    }
    
    // Remove .bin extension if present
    char app_name[64];
    strncpy(app_name, filename, sizeof(app_name) - 1);
    app_name[sizeof(app_name) - 1] = '\0';
    
    char *dot = strrchr(app_name, '.');
    if (dot != NULL && strcmp(dot, ".bin") == 0) {
        *dot = '\0';
    }
    
    // Read the app binary from storage
    void *app_data = NULL;
    size_t app_size = 0;
    
    // Load from file system using app_storage (expects app name, not full path)
    esp_err_t ret = app_storage_load(app_name, &app_data, &app_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load app '%s' from storage: %s", app_name, esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ Read %zu bytes from storage", app_size);
    
    // Get registry and lock
    app_registry_t *registry = app_get_registry();
    ret = app_registry_lock();
    if (ret != ESP_OK) {
        free(app_data);
        return ret;
    }
    
    // Find a free slot
    int slot = -1;
    for (int i = 0; i < APP_REGISTRY_MAX_ENTRIES; i++) {
        if (!registry->entries[i].registered) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        app_registry_unlock();
        free(app_data);
        ESP_LOGE(TAG, "Maximum number of apps reached");
        return ESP_ERR_NO_MEM;
    }
    
    // Load the binary into app_info
    app_info_t *info = &registry->entries[slot].info;
    ret = app_load_binary(app_data, app_size, info);
    free(app_data);  // Free the temporary buffer
    
    if (ret != ESP_OK) {
        app_registry_unlock();
        return ret;
    }
    
    // Set source type and mark as dynamic
    info->source = APP_SOURCE_STORAGE;
    info->is_dynamic = true;
    
    // Mark as registered
    registry->entries[slot].registered = true;
    registry->count++;
    
    app_registry_unlock();
    
    *out_info = info;
    
    ESP_LOGI(TAG, "✓ Successfully loaded app '%s' from storage", info->manifest.name);
    
    return ESP_OK;
}

esp_err_t app_manager_load_from_partition(const char *partition_label, size_t offset, app_info_t **out_info)
{
    if (partition_label == NULL || out_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Loading app from partition '%s' at offset 0x%zx", partition_label, offset);
    
    // Find the partition
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_FAT,
        partition_label
    );
    
    if (partition == NULL) {
        ESP_LOGE(TAG, "Partition '%s' not found", partition_label);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Found partition '%s' at 0x%lx, size=%lu", 
             partition_label, partition->address, partition->size);
    
    // Check if offset is valid
    if (offset >= partition->size) {
        ESP_LOGE(TAG, "Offset 0x%zx is beyond partition size %lu", offset, partition->size);
        return ESP_ERR_INVALID_SIZE;
    }
    
    ESP_LOGI(TAG, "Reading from partition offset 0x%zx (absolute: 0x%lx)", 
             offset, partition->address + offset);
    
    // Read app header first to get size
    uint8_t header_buf[128];
    memset(header_buf, 0, sizeof(header_buf));
    esp_err_t ret = esp_partition_read(partition, offset, header_buf, sizeof(header_buf));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read header: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Debug: print first 16 bytes
    ESP_LOGI(TAG, "First 16 bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
             header_buf[0], header_buf[1], header_buf[2], header_buf[3],
             header_buf[4], header_buf[5], header_buf[6], header_buf[7],
             header_buf[8], header_buf[9], header_buf[10], header_buf[11],
             header_buf[12], header_buf[13], header_buf[14], header_buf[15]);
    
    // Check magic (0x4150504B = "APPK")
    uint32_t magic = *(uint32_t*)&header_buf[0];
    if (magic != 0x4150504B) {
        ESP_LOGE(TAG, "Invalid magic: 0x%08lX (expected 0x4150504B)", magic);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get app size from header
    uint32_t app_size = *(uint32_t*)&header_buf[84];
    size_t total_size = 128 + app_size;
    
    ESP_LOGI(TAG, "App size: %lu bytes (total with header: %zu)", app_size, total_size);
    
    // Allocate memory for entire app
    void *app_data = malloc(total_size);
    if (app_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes", total_size);
        return ESP_ERR_NO_MEM;
    }
    
    // Read the entire app
    ret = esp_partition_read(partition, offset, app_data, total_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read app: %s", esp_err_to_name(ret));
        free(app_data);
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ Loaded %zu bytes from partition", total_size);
    
    // Get registry and lock
    app_registry_t *registry = app_get_registry();
    ret = app_registry_lock();
    if (ret != ESP_OK) {
        free(app_data);
        return ret;
    }
    
    // Find a free slot
    int slot = -1;
    for (int i = 0; i < APP_REGISTRY_MAX_ENTRIES; i++) {
        if (!registry->entries[i].registered) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        app_registry_unlock();
        free(app_data);
        ESP_LOGE(TAG, "Maximum number of apps reached");
        return ESP_ERR_NO_MEM;
    }
    
    // Load the binary into app_info
    app_info_t *info = &registry->entries[slot].info;
    ret = app_load_binary(app_data, total_size, info);
    free(app_data);  // Free the temporary buffer
    
    if (ret != ESP_OK) {
        app_registry_unlock();
        return ret;
    }
    
    // Set source type and mark as dynamic
    info->source = APP_SOURCE_STORAGE;
    info->is_dynamic = true;
    
    // Mark as registered
    registry->entries[slot].registered = true;
    registry->count++;
    
    app_registry_unlock();
    
    *out_info = info;
    
    ESP_LOGI(TAG, "✓ Successfully loaded app '%s' from partition", info->manifest.name);
    
    return ESP_OK;
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
