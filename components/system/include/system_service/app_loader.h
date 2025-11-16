#ifndef APP_LOADER_H
#define APP_LOADER_H

#include "esp_err.h"
#include "system_service/app_manager.h"
#include "spi_flash_mmap.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief System API Table - stable interface exported to dynamic apps
 * 
 * This table contains function pointers to system services that dynamic apps
 * can call. The table structure must remain stable across versions (append-only).
 */
typedef struct {
    uint32_t version;  // API version for compatibility checking
    
    // Service management
    esp_err_t (*register_service)(const char *name, void *ctx, system_service_id_t *id);
    esp_err_t (*unregister_service)(system_service_id_t id);
    esp_err_t (*set_state)(system_service_id_t id, system_service_state_t state);
    esp_err_t (*heartbeat)(system_service_id_t id);
    
    // Event bus
    esp_err_t (*post_event)(system_service_id_t id, system_event_type_t type,
                           const void *data, size_t size, system_event_priority_t priority);
    esp_err_t (*subscribe_event)(system_service_id_t id, system_event_type_t type,
                                system_event_handler_t handler, void *user_data);
    esp_err_t (*unsubscribe_event)(system_service_id_t id, system_event_type_t type);
    esp_err_t (*register_event_type)(const char *name, system_event_type_t *out_type);
    
    // Memory allocation (apps use PSRAM)
    void* (*malloc)(size_t size);
    void (*free)(void *ptr);
    void* (*calloc)(size_t nmemb, size_t size);
    void* (*realloc)(void *ptr, size_t size);
    
    // Logging
    void (*log_write)(uint32_t level, const char *tag, const char *format, ...);
    
    // FreeRTOS task functions
    void (*task_delay)(uint32_t ms);
    uint32_t (*get_tick_count)(void);
    
} system_api_table_t;

/**
 * @brief ELF file header (simplified for ESP32)
 */
typedef struct {
    uint8_t  e_ident[16];    // Magic number and other info
    uint16_t e_type;         // Object file type
    uint16_t e_machine;      // Architecture
    uint32_t e_version;      // Object file version
    uint32_t e_entry;        // Entry point virtual address
    uint32_t e_phoff;        // Program header table file offset
    uint32_t e_shoff;        // Section header table file offset
    uint32_t e_flags;        // Processor-specific flags
    uint16_t e_ehsize;       // ELF header size in bytes
    uint16_t e_phentsize;    // Program header table entry size
    uint16_t e_phnum;        // Program header table entry count
    uint16_t e_shentsize;    // Section header table entry size
    uint16_t e_shnum;        // Section header table entry count
    uint16_t e_shstrndx;     // Section header string table index
} elf32_ehdr_t;

/**
 * @brief ELF section header
 */
typedef struct {
    uint32_t sh_name;        // Section name (string tbl index)
    uint32_t sh_type;        // Section type
    uint32_t sh_flags;       // Section flags
    uint32_t sh_addr;        // Section virtual addr at execution
    uint32_t sh_offset;      // Section file offset
    uint32_t sh_size;        // Section size in bytes
    uint32_t sh_link;        // Link to another section
    uint32_t sh_info;        // Additional section information
    uint32_t sh_addralign;   // Section alignment
    uint32_t sh_entsize;     // Entry size if section holds table
} elf32_shdr_t;

/**
 * @brief ELF relocation entry with addend
 */
typedef struct {
    uint32_t r_offset;       // Address
    uint32_t r_info;         // Relocation type and symbol index
    int32_t  r_addend;       // Addend
} elf32_rela_t;

/**
 * @brief ELF symbol table entry
 */
typedef struct {
    uint32_t st_name;        // Symbol name (string tbl index)
    uint32_t st_value;       // Symbol value
    uint32_t st_size;        // Symbol size
    uint8_t  st_info;        // Symbol type and binding
    uint8_t  st_other;       // Symbol visibility
    uint16_t st_shndx;       // Section index
} elf32_sym_t;

/**
 * @brief Loaded app binary information
 */
typedef struct {
    void *code_segment;      // Pointer to executable code (IRAM or flash-mapped)
    void *data_segment;      // Pointer to data section (RAM)
    void *bss_segment;       // Pointer to BSS section (RAM)
    size_t code_size;
    size_t data_size;
    size_t bss_size;
    
    void *entry_point;       // App entry function
    void *exit_point;        // App exit function (optional)
    void *manifest_ptr;      // Pointer to app manifest (if found in symbols)
    
    system_api_table_t *api_table;  // System API table for app
    
    // Flash memory mapping (for XIP - execute in place)
    spi_flash_mmap_handle_t mmap_handle;  // Handle for mapped flash region
    bool code_in_flash;      // True if code is executed from flash (XIP)
    
    // PSRAM D-cache buffer (when using I-cache mirror for execution)
    void *psram_data_buf;    // D-cache address for data access
} loaded_app_t;

/**
 * @brief Initialize the app loader subsystem
 * 
 * Sets up the system API table with stable function pointers.
 * 
 * @return ESP_OK on success
 */
esp_err_t app_loader_init(void);

/**
 * @brief Get the system API table
 * 
 * Returns a pointer to the stable system API table that can be passed to apps.
 * 
 * @return Pointer to system API table
 */
const system_api_table_t* app_loader_get_api_table(void);

/**
 * @brief Load a dynamic app from a binary buffer
 * 
 * Loads an ELF binary, allocates memory, applies relocations, and resolves symbols.
 * 
 * @param binary Pointer to ELF binary data
 * @param size Size of binary data
 * @param out_app Output loaded app structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_loader_load_binary(const void *binary, size_t size, loaded_app_t *out_app);

/**
 * @brief Load a dynamic app from a flash partition
 * 
 * Loads an app from a flash partition, using memory mapping for code sections
 * to avoid copying to RAM.
 * 
 * @param partition_label Partition label (e.g., "app_store")
 * @param offset Offset within partition
 * @param out_app Output loaded app structure
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_loader_load_from_partition(const char *partition_label, size_t offset, loaded_app_t *out_app);

/**
 * @brief Unload a dynamic app
 * 
 * Frees all memory allocated for the app and unmaps flash regions.
 * 
 * @param app Loaded app structure
 * @return ESP_OK on success
 */
esp_err_t app_loader_unload(loaded_app_t *app);

/**
 * @brief Apply relocations to a loaded app
 * 
 * Processes ELF relocation entries and adjusts addresses.
 * 
 * @param app Loaded app structure
 * @param elf_data ELF file data
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_loader_apply_relocations(loaded_app_t *app, const void *elf_data);

/**
 * @brief Resolve external symbols
 * 
 * Resolves symbols that the app references from the system API table.
 * 
 * @param app Loaded app structure
 * @param elf_data ELF file data
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t app_loader_resolve_symbols(loaded_app_t *app, const void *elf_data);

#ifdef __cplusplus
}
#endif

#endif // APP_LOADER_H
