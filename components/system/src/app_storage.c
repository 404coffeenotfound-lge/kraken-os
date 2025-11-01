#include "system_service/app_storage.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <dirent.h>

static const char *TAG = "app_storage";
static bool storage_initialized = false;
static bool storage_mounted = false;

esp_err_t app_storage_init(void)
{
    if (storage_initialized) {
        ESP_LOGW(TAG, "App storage already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing app storage...");
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = APP_STORAGE_BASE_PATH,
        .partition_label = "storage",
        .max_files = 10,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS partition size: total: %d, used: %d", total, used);
    }
    
    storage_initialized = true;
    storage_mounted = true;
    
    ESP_LOGI(TAG, "✓ App storage initialized");
    
    return ESP_OK;
}

esp_err_t app_storage_mount(void)
{
    if (!storage_initialized) {
        return app_storage_init();
    }
    
    if (storage_mounted) {
        ESP_LOGW(TAG, "Storage already mounted");
        return ESP_OK;
    }
    
    storage_mounted = true;
    return ESP_OK;
}

esp_err_t app_storage_unmount(void)
{
    if (!storage_mounted) {
        return ESP_OK;
    }
    
    esp_err_t ret = esp_vfs_spiffs_unregister("storage");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount storage");
        return ret;
    }
    
    storage_mounted = false;
    ESP_LOGI(TAG, "✓ Storage unmounted");
    
    return ESP_OK;
}

esp_err_t app_storage_save(const char *app_name, const void *data, size_t size)
{
    if (app_name == NULL || data == NULL || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!storage_mounted) {
        ESP_LOGE(TAG, "Storage not mounted");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (size > APP_STORAGE_MAX_SIZE) {
        ESP_LOGE(TAG, "App size %d exceeds maximum %d", size, APP_STORAGE_MAX_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }
    
    char path[300];
    snprintf(path, sizeof(path), "%s/%s.bin", APP_STORAGE_BASE_PATH, app_name);
    
    ESP_LOGI(TAG, "Saving app '%s' (%d bytes) to %s", app_name, size, path);
    
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
        return ESP_FAIL;
    }
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    if (written != size) {
        ESP_LOGE(TAG, "Failed to write complete data: %d/%d bytes", written, size);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✓ Saved app '%s' successfully", app_name);
    
    return ESP_OK;
}

esp_err_t app_storage_load(const char *app_name, void **out_data, size_t *out_size)
{
    if (app_name == NULL || out_data == NULL || out_size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!storage_mounted) {
        ESP_LOGE(TAG, "Storage not mounted");
        return ESP_ERR_INVALID_STATE;
    }
    
    char path[300];
    snprintf(path, sizeof(path), "%s/%s.bin", APP_STORAGE_BASE_PATH, app_name);
    
    ESP_LOGI(TAG, "Loading app '%s' from %s", app_name, path);
    
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size == 0 || size > APP_STORAGE_MAX_SIZE) {
        ESP_LOGE(TAG, "Invalid file size: %d", size);
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Allocate memory
    void *data = malloc(size);
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes", size);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    
    // Read file
    size_t read = fread(data, 1, size, f);
    fclose(f);
    
    if (read != size) {
        ESP_LOGE(TAG, "Failed to read complete file: %d/%d bytes", read, size);
        free(data);
        return ESP_FAIL;
    }
    
    *out_data = data;
    *out_size = size;
    
    ESP_LOGI(TAG, "✓ Loaded app '%s' (%d bytes)", app_name, size);
    
    return ESP_OK;
}

esp_err_t app_storage_delete(const char *app_name)
{
    if (app_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!storage_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    
    char path[300];
    snprintf(path, sizeof(path), "%s/%s.bin", APP_STORAGE_BASE_PATH, app_name);
    
    if (unlink(path) != 0) {
        ESP_LOGE(TAG, "Failed to delete: %s", path);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✓ Deleted app '%s'", app_name);
    
    return ESP_OK;
}

esp_err_t app_storage_exists(const char *app_name, bool *exists)
{
    if (app_name == NULL || exists == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!storage_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    
    char path[128];
    snprintf(path, sizeof(path), "%s/%s.bin", APP_STORAGE_BASE_PATH, app_name);
    
    struct stat st;
    *exists = (stat(path, &st) == 0);
    
    return ESP_OK;
}

esp_err_t app_storage_list(app_storage_entry_t *entries, size_t max_count, size_t *out_count)
{
    if (entries == NULL || out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!storage_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    
    DIR *dir = opendir(APP_STORAGE_BASE_PATH);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open directory: %s", APP_STORAGE_BASE_PATH);
        return ESP_FAIL;
    }
    
    size_t count = 0;
    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL && count < max_count) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Only process .bin files
        size_t name_len = strlen(entry->d_name);
        if (name_len < 5 || strcmp(entry->d_name + name_len - 4, ".bin") != 0) {
            continue;
        }
        
        // Get file stats
        char path[300];
        snprintf(path, sizeof(path), "%s/%s", APP_STORAGE_BASE_PATH, entry->d_name);
        
        struct stat st;
        if (stat(path, &st) != 0) {
            continue;
        }
        
        // Fill entry
        strncpy(entries[count].name, entry->d_name, sizeof(entries[count].name) - 1);
        entries[count].name[name_len - 4] = '\0';  // Remove .bin extension
        
        strncpy(entries[count].path, path, sizeof(entries[count].path) - 1);
        entries[count].size = st.st_size;
        entries[count].crc32 = 0;  // TODO: Calculate CRC32
        entries[count].install_time = st.st_mtime;
        
        count++;
    }
    
    closedir(dir);
    
    *out_count = count;
    
    ESP_LOGI(TAG, "Found %d apps in storage", count);
    
    return ESP_OK;
}

esp_err_t app_storage_get_info(const char *app_name, app_storage_entry_t *info)
{
    if (app_name == NULL || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!storage_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    
    char path[300];
    snprintf(path, sizeof(path), "%s/%s.bin", APP_STORAGE_BASE_PATH, app_name);
    
    struct stat st;
    if (stat(path, &st) != 0) {
        return ESP_ERR_NOT_FOUND;
    }
    
    strncpy(info->name, app_name, sizeof(info->name) - 1);
    strncpy(info->path, path, sizeof(info->path) - 1);
    info->size = st.st_size;
    info->crc32 = 0;  // TODO: Calculate CRC32
    info->install_time = st.st_mtime;
    
    return ESP_OK;
}

esp_err_t app_storage_get_free_space(size_t *free_bytes)
{
    if (free_bytes == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!storage_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info("storage", &total, &used);
    if (ret != ESP_OK) {
        return ret;
    }
    
    *free_bytes = total - used;
    
    return ESP_OK;
}
