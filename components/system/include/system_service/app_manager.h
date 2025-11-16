#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include "esp_err.h"
#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_MAX_NAME_LEN        32
#define APP_MAX_VERSION_LEN     16
#define APP_MAX_AUTHOR_LEN      32
#define APP_MAX_APPS            16
#define APP_MAGIC_NUMBER        0x4150504B  // "APPK"

typedef enum {
    APP_STATE_UNLOADED = 0,
    APP_STATE_LOADED,
    APP_STATE_RUNNING,
    APP_STATE_PAUSED,
    APP_STATE_ERROR,
} app_state_t;

typedef enum {
    APP_SOURCE_INTERNAL = 0,    // Built-in / compiled in
    APP_SOURCE_STORAGE,         // Loaded from storage (SPIFFS/FAT)
    APP_SOURCE_REMOTE,          // Downloaded from URL
} app_source_t;

typedef struct {
    uint32_t magic;
    char name[APP_MAX_NAME_LEN];
    char version[APP_MAX_VERSION_LEN];
    char author[APP_MAX_AUTHOR_LEN];
    uint32_t size;
    uint32_t entry_point;       // Offset to entry function
    uint32_t crc32;
} app_header_t;

typedef struct app_context app_context_t;

typedef esp_err_t (*app_entry_fn_t)(app_context_t *ctx);
typedef esp_err_t (*app_exit_fn_t)(app_context_t *ctx);

typedef struct {
    char name[APP_MAX_NAME_LEN];
    char version[APP_MAX_VERSION_LEN];
    char author[APP_MAX_AUTHOR_LEN];
    
    app_entry_fn_t entry;
    app_exit_fn_t exit;
    
    void *user_data;
} app_manifest_t;

typedef struct {
    app_manifest_t manifest;
    app_state_t state;
    app_source_t source;
    
    void *app_data;
    size_t app_size;
    
    system_service_id_t service_id;
    uint32_t load_time;
    
    bool is_dynamic;            // true if loaded from storage/remote
} app_info_t;

struct app_context {
    app_info_t *app_info;
    system_service_id_t service_id;
    
    // Full system service API access (apps are first-class citizens)
    // Service management
    esp_err_t (*register_service)(const char *name, void *ctx, system_service_id_t *id);
    esp_err_t (*unregister_service)(system_service_id_t id);
    esp_err_t (*set_state)(system_service_id_t id, system_service_state_t state);
    esp_err_t (*heartbeat)(system_service_id_t id);
    
    // Event bus - full access
    esp_err_t (*post_event)(system_service_id_t id, system_event_type_t type, 
                           const void *data, size_t size, system_event_priority_t priority);
    esp_err_t (*subscribe_event)(system_service_id_t id, system_event_type_t type,
                                system_event_handler_t handler, void *user_data);
    esp_err_t (*unsubscribe_event)(system_service_id_t id, system_event_type_t type);
    esp_err_t (*register_event_type)(const char *name, system_event_type_t *out_type);
};

esp_err_t app_manager_init(void);

esp_err_t app_manager_register_app(const app_manifest_t *manifest, app_info_t **out_info);

esp_err_t app_manager_load_from_storage(const char *path, app_info_t **out_info);

esp_err_t app_manager_load_from_partition(const char *partition_label, size_t offset, app_info_t **out_info);

esp_err_t app_manager_load_from_url(const char *url, app_info_t **out_info);

esp_err_t app_manager_install(const void *app_data, size_t size, 
                              const char *install_path, app_info_t **out_info);

esp_err_t app_manager_uninstall(const char *app_name);

esp_err_t app_manager_start_app(const char *app_name);

esp_err_t app_manager_stop_app(const char *app_name);

esp_err_t app_manager_pause_app(const char *app_name);

esp_err_t app_manager_resume_app(const char *app_name);

esp_err_t app_manager_get_info(const char *app_name, app_info_t *out_info);

esp_err_t app_manager_list_apps(app_info_t *apps, size_t max_count, size_t *out_count);

esp_err_t app_manager_get_running_apps(app_info_t *apps, size_t max_count, size_t *out_count);

/**
 * @brief Load and register a dynamic app from flash partition using ELF loader
 * 
 * This function loads a Position-Independent Code (PIC) app from a flash partition,
 * parses the ELF binary, applies relocations, and registers it with the app manager.
 * 
 * @param partition_label Flash partition label (e.g., "app_store")
 * @param offset Offset within partition where app ELF binary starts
 * @param out_info Output pointer to app info (optional, can be NULL)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_manager_load_dynamic_from_partition(const char *partition_label, size_t offset, app_info_t **out_info);

/**
 * @brief Load app from flash partition using memory mapping
 * 
 * This function loads an app directly from flash and maps the code section
 * into instruction cache for execution. This avoids PSRAM execution limitations.
 * 
 * @param partition_label Flash partition label (e.g., "app_store")
 * @param offset Offset within partition where app binary starts
 * @param info Output app info structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t flash_app_load(const char *partition_label, size_t offset, app_info_t *info);

#ifdef __cplusplus
}
#endif

#endif // APP_MANAGER_H
