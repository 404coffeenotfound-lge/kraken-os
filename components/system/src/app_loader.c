/**
 * @file app_loader.c
 * @brief Dynamic app loader implementation for Kraken OS
 * 
 * Implements Position-Independent Code (PIC) loading with ELF relocation support.
 */

#include "system_service/app_loader.h"
#include "system_service/memory_utils.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "spi_flash_mmap.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

static const char *TAG = "app_loader";

// ELF constants
#define ELF_MAGIC       0x464C457F  // "\x7FELF"
#define EM_XTENSA       94          // Xtensa architecture
#define ET_DYN          3           // Shared object (PIC)
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_RELA        4
#define SHT_HASH        5           // Symbol hash table
#define SHT_DYNAMIC     6           // Dynamic linking information
#define SHT_NOBITS      8
#define SHT_DYNSYM      11          // Dynamic symbol table
#define SHF_ALLOC       0x2
#define SHF_EXECINSTR   0x4
#define SHF_WRITE       0x1

// Xtensa relocation types (from xtensa elf.h)
#define R_XTENSA_NONE       0
#define R_XTENSA_32         1
#define R_XTENSA_ASM_EXPAND 11
#define R_XTENSA_SLOT0_OP   20
#define R_XTENSA_RELATIVE   2
#define R_XTENSA_JMP_SLOT   4
#define R_XTENSA_GLOB_DAT   3
#define R_XTENSA_RTLD       5

// System API table (stable interface for apps)
static system_api_table_t g_system_api_table;

// Forward declarations
static esp_err_t parse_elf_header(const void *binary, size_t size, elf32_ehdr_t *ehdr);
static esp_err_t load_sections(const void *binary, loaded_app_t *app);
static esp_err_t load_data_sections_only(const uint8_t *elf_data, loaded_app_t *app);
static void* map_elf_addr_to_loaded(uint32_t elf_addr);
static void* resolve_external_symbol(const char *sym_name);

// Memory wrappers for API table
static void* api_malloc_prefer_psram(size_t size)
{
    return memory_alloc_prefer_psram(size);
}

static void* api_calloc_prefer_psram(size_t nmemb, size_t size)
{
    void *ptr = memory_alloc_prefer_psram(nmemb * size);
    if (ptr) {
        memset(ptr, 0, nmemb * size);
    }
    return ptr;
}

static void* api_realloc_prefer_psram(void *ptr, size_t size)
{
    // Simple realloc - allocate new, copy, free old
    if (!ptr) {
        return memory_alloc_prefer_psram(size);
    }
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    
    void *new_ptr = memory_alloc_prefer_psram(size);
    if (new_ptr && ptr) {
        // We don't know the old size, so we can't safely copy
        // This is a limitation - apps should avoid realloc
        ESP_LOGW(TAG, "realloc not fully supported, may lose data");
    }
    return new_ptr;
}

/**
 * @brief Logging wrapper for apps
 */
static void app_log_write(uint32_t level, const char *tag, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    esp_log_writev((esp_log_level_t)level, tag, format, args);
    va_end(args);
}

/**
 * @brief Task delay wrapper (ms to ticks)
 */
static void app_task_delay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * @brief Get tick count wrapper
 */
static uint32_t app_get_tick_count(void)
{
    return xTaskGetTickCount();
}

/**
 * @brief Initialize the app loader and system API table
 */
esp_err_t app_loader_init(void)
{
    ESP_LOGI(TAG, "Initializing app loader...");
    
    // Initialize system API table with stable function pointers
    g_system_api_table.version = 1;  // API version
    
    // Service management APIs
    g_system_api_table.register_service = system_service_register;
    g_system_api_table.unregister_service = system_service_unregister;
    g_system_api_table.set_state = system_service_set_state;
    g_system_api_table.heartbeat = system_service_heartbeat;
    
    // Event bus APIs
    g_system_api_table.post_event = system_event_post;
    g_system_api_table.subscribe_event = system_event_subscribe;
    g_system_api_table.unsubscribe_event = system_event_unsubscribe;
    g_system_api_table.register_event_type = system_event_register_type;
    
    // Memory management (apps use PSRAM)
    g_system_api_table.malloc = api_malloc_prefer_psram;
    g_system_api_table.free = free;
    g_system_api_table.calloc = api_calloc_prefer_psram;
    g_system_api_table.realloc = api_realloc_prefer_psram;
    
    // Logging
    g_system_api_table.log_write = app_log_write;
    
    // FreeRTOS
    g_system_api_table.task_delay = app_task_delay;
    g_system_api_table.get_tick_count = app_get_tick_count;
    
    ESP_LOGI(TAG, "App loader initialized (API version %lu)", g_system_api_table.version);
    return ESP_OK;
}

/**
 * @brief Get the system API table
 */
const system_api_table_t* app_loader_get_api_table(void)
{
    return &g_system_api_table;
}

/**
 * @brief Parse ELF header and validate
 */
static esp_err_t parse_elf_header(const void *binary, size_t size, elf32_ehdr_t *ehdr)
{
    if (size < sizeof(elf32_ehdr_t)) {
        ESP_LOGE(TAG, "Binary too small for ELF header");
        return ESP_ERR_INVALID_SIZE;
    }
    
    memcpy(ehdr, binary, sizeof(elf32_ehdr_t));
    
    // Validate ELF magic
    uint32_t magic = *(uint32_t*)ehdr->e_ident;
    if (magic != ELF_MAGIC) {
        ESP_LOGE(TAG, "Invalid ELF magic: 0x%08lX (expected 0x%08X)", magic, ELF_MAGIC);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate architecture
    if (ehdr->e_machine != EM_XTENSA) {
        ESP_LOGE(TAG, "Invalid architecture: %u (expected Xtensa %u)", ehdr->e_machine, EM_XTENSA);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    // Validate file type (should be ET_DYN for PIC)
    if (ehdr->e_type != ET_DYN) {
        ESP_LOGW(TAG, "Warning: ELF type is %u (expected ET_DYN=%u for PIC)", ehdr->e_type, ET_DYN);
    }
    
    ESP_LOGI(TAG, "ELF header valid:");
    ESP_LOGI(TAG, "  Type:         %u", ehdr->e_type);
    ESP_LOGI(TAG, "  Machine:      %u (Xtensa)", ehdr->e_machine);
    ESP_LOGI(TAG, "  Entry:        0x%08lX", ehdr->e_entry);
    ESP_LOGI(TAG, "  Sections:     %u", ehdr->e_shnum);
    
    return ESP_OK;
}

// Section mapping table (to map ELF virtual addresses to loaded addresses)
typedef struct {
    uint32_t elf_addr;      // Virtual address in ELF
    void *loaded_addr;       // Actual loaded address in RAM
    size_t size;
} section_mapping_t;

static section_mapping_t g_section_map[32];  // Max 32 sections
static int g_section_map_count = 0;

/**
 * @brief Load only data sections to RAM (for XIP mode where code stays in flash)
 */
static esp_err_t load_data_sections_only(const uint8_t *elf_data, loaded_app_t *app)
{
    elf32_ehdr_t *ehdr = (elf32_ehdr_t*)elf_data;
    elf32_shdr_t *shdr_table = (elf32_shdr_t*)(elf_data + ehdr->e_shoff);
    const char *shstrtab = (const char*)(elf_data + shdr_table[ehdr->e_shstrndx].sh_offset);
    
    // Reset section map
    g_section_map_count = 0;
    
    // First pass: calculate total data size needed
    app->data_size = 0;
    app->bss_size = 0;
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        elf32_shdr_t *shdr = &shdr_table[i];
        
        if (!(shdr->sh_flags & SHF_ALLOC)) {
            continue;  // Skip non-allocatable sections
        }
        
        // Skip executable sections (they stay in flash)
        if (shdr->sh_flags & SHF_EXECINSTR) {
            // But still register code section mappings to flash address
            if (g_section_map_count < 32) {
                g_section_map[g_section_map_count].elf_addr = shdr->sh_addr;
                g_section_map[g_section_map_count].loaded_addr = (uint8_t*)app->code_segment + (shdr->sh_addr - shdr_table[1].sh_addr);
                g_section_map[g_section_map_count].size = shdr->sh_size;
                const char *section_name = shstrtab + shdr->sh_name;
                ESP_LOGI(TAG, "  Map: ELF 0x%08lX -> Flash %p (%s, %lu bytes)",
                         shdr->sh_addr, g_section_map[g_section_map_count].loaded_addr, 
                         section_name, shdr->sh_size);
                g_section_map_count++;
            }
            continue;
        }
        
        if (shdr->sh_type == SHT_PROGBITS || shdr->sh_type == SHT_DYNAMIC ||
            shdr->sh_type == SHT_DYNSYM || shdr->sh_type == SHT_HASH ||
            shdr->sh_type == SHT_RELA) {
            app->data_size += shdr->sh_size;
        } else if (shdr->sh_type == SHT_NOBITS) {
            app->bss_size += shdr->sh_size;
        }
    }
    
    ESP_LOGI(TAG, "Memory requirements:");
    ESP_LOGI(TAG, "  Code: %zu bytes (in flash, XIP)", app->code_size);
    ESP_LOGI(TAG, "  Data: %zu bytes", app->data_size);
    ESP_LOGI(TAG, "  BSS:  %zu bytes", app->bss_size);
    
    // Allocate RAM for data sections
    if (app->data_size > 0) {
        app->data_segment = heap_caps_malloc(app->data_size, MALLOC_CAP_8BIT);
        if (!app->data_segment) {
            ESP_LOGE(TAG, "Failed to allocate %zu bytes for data", app->data_size);
            return ESP_ERR_NO_MEM;
        }
        
        // Copy data sections and record mappings
        size_t data_offset = 0;
        for (int i = 0; i < ehdr->e_shnum; i++) {
            elf32_shdr_t *shdr = &shdr_table[i];
            const char *section_name = shstrtab + shdr->sh_name;
            
            if ((shdr->sh_flags & SHF_ALLOC) && 
                !(shdr->sh_flags & SHF_EXECINSTR) &&
                (shdr->sh_type != SHT_NOBITS)) {
                
                void *dest = (uint8_t*)app->data_segment + data_offset;
                memcpy(dest, elf_data + shdr->sh_offset, shdr->sh_size);
                
                // Record section mapping
                if (g_section_map_count < 32) {
                    g_section_map[g_section_map_count].elf_addr = shdr->sh_addr;
                    g_section_map[g_section_map_count].loaded_addr = dest;
                    g_section_map[g_section_map_count].size = shdr->sh_size;
                    ESP_LOGI(TAG, "  Map: ELF 0x%08lX -> RAM %p (%s, %lu bytes)",
                             shdr->sh_addr, dest, section_name, shdr->sh_size);
                    g_section_map_count++;
                }
                
                data_offset += shdr->sh_size;
            }
        }
        
        ESP_LOGI(TAG, "✓ Data loaded at %p (%zu bytes)", app->data_segment, app->data_size);
    }
    
    // Allocate and zero BSS
    if (app->bss_size > 0) {
        app->bss_segment = heap_caps_malloc(app->bss_size, MALLOC_CAP_8BIT);
        if (!app->bss_segment) {
            ESP_LOGE(TAG, "Failed to allocate %zu bytes for BSS", app->bss_size);
            if (app->data_segment) free(app->data_segment);
            return ESP_ERR_NO_MEM;
        }
        memset(app->bss_segment, 0, app->bss_size);
        ESP_LOGI(TAG, "✓ BSS allocated at %p (%zu bytes)", app->bss_segment, app->bss_size);
    }
    
    ESP_LOGI(TAG, "Total section mappings: %d", g_section_map_count);
    
    return ESP_OK;
}

/**
 * @brief Load ELF sections - Hybrid version with proper address mapping
 * 
 * Code copied from ELF to executable RAM (IRAM), data/bss in RAM.
 * CRITICAL: We must maintain section layout to make relocations work correctly.
 * Each section is loaded at its own location, and we track the mapping.
 */
static esp_err_t load_sections_hybrid(const uint8_t *elf_data, loaded_app_t *app)
{
    elf32_ehdr_t *ehdr = (elf32_ehdr_t*)elf_data;
    elf32_shdr_t *shdr_table = (elf32_shdr_t*)(elf_data + ehdr->e_shoff);
    
    // Get section header string table
    elf32_shdr_t *shstrtab_shdr = &shdr_table[ehdr->e_shstrndx];
    const char *shstrtab = (const char*)(elf_data + shstrtab_shdr->sh_offset);
    
    // Reset section mapping
    g_section_map_count = 0;
    memset(g_section_map, 0, sizeof(g_section_map));
    
    // Calculate total sizes needed
    app->code_size = 0;
    app->data_size = 0;
    app->bss_size = 0;
    
    ESP_LOGI(TAG, "Analyzing ELF sections:");
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        elf32_shdr_t *shdr = &shdr_table[i];
        const char *section_name = shstrtab + shdr->sh_name;
        
        if (!(shdr->sh_flags & SHF_ALLOC)) {
            continue;  // Skip non-allocatable sections
        }
        
        if (shdr->sh_flags & SHF_EXECINSTR) {
            // Executable section (.text)
            app->code_size += shdr->sh_size;
            ESP_LOGI(TAG, "  [%d] %s: EXEC, addr=0x%08lX, size=%lu", 
                     i, section_name, shdr->sh_addr, shdr->sh_size);
        } else if (shdr->sh_type == SHT_NOBITS) {
            // BSS section (uninitialized)
            app->bss_size += shdr->sh_size;
            ESP_LOGI(TAG, "  [%d] %s: BSS, addr=0x%08lX, size=%lu", 
                     i, section_name, shdr->sh_addr, shdr->sh_size);
        } else {
            // Data section (.rodata, .data, .got, etc.)
            app->data_size += shdr->sh_size;
            ESP_LOGI(TAG, "  [%d] %s: DATA, addr=0x%08lX, size=%lu", 
                     i, section_name, shdr->sh_addr, shdr->sh_size);
        }
    }
    
    ESP_LOGI(TAG, "Memory requirements:");
    ESP_LOGI(TAG, "  Code: %zu bytes", app->code_size);
    ESP_LOGI(TAG, "  Data: %zu bytes", app->data_size);
    ESP_LOGI(TAG, "  BSS:  %zu bytes", app->bss_size);
    
    // Allocate executable RAM for code
    if (app->code_size > 0) {
        void *code_mem = NULL;
        void *data_buf = NULL;  // For PSRAM: separate data buffer
        
        // ESP32-S3: Try IRAM first, then fall back to PSRAM
        // PSRAM can execute via instruction cache, but has literal pool access issues
        size_t free_iram = heap_caps_get_free_size(MALLOC_CAP_EXEC | MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Available IRAM: %zu bytes (need %zu bytes)", free_iram, app->code_size);
        
        code_mem = heap_caps_malloc(app->code_size, MALLOC_CAP_EXEC | MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT);
        
        if (!code_mem) {
            // IRAM full - use PSRAM for code execution
            // Allocate from PSRAM with EXEC capability for instruction fetch
            ESP_LOGI(TAG, "IRAM full, using PSRAM I-cache for code execution");
            
            // Allocate executable PSRAM - this requires CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y
            code_mem = heap_caps_malloc(app->code_size, MALLOC_CAP_EXEC | MALLOC_CAP_SPIRAM);
            
            if (!code_mem) {
                ESP_LOGE(TAG, "Failed to allocate %zu bytes for code in PSRAM", app->code_size);
                ESP_LOGE(TAG, "Ensure CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y is enabled");
                return ESP_ERR_NO_MEM;
            }
            
            ESP_LOGI(TAG, "PSRAM executable: %p", code_mem);
        }
        
        app->code_segment = code_mem;
        app->code_in_flash = false;
        ESP_LOGI(TAG, "Code ready at %p (%zu bytes)", code_mem, app->code_size);
        
        // Copy executable sections and record mappings
        size_t code_offset = 0;
        for (int i = 0; i < ehdr->e_shnum; i++) {
            elf32_shdr_t *shdr = &shdr_table[i];
            const char *section_name = shstrtab + shdr->sh_name;
            
            if ((shdr->sh_flags & SHF_ALLOC) && (shdr->sh_flags & SHF_EXECINSTR)) {
                // Copy code to allocated memory
                void *copy_dest = ((uint8_t*)code_mem + code_offset);
                    
                memcpy(copy_dest, elf_data + shdr->sh_offset, shdr->sh_size);
                
                // Record section mapping: ELF virtual address -> RAM address
                if (g_section_map_count < 32) {
                    g_section_map[g_section_map_count].elf_addr = shdr->sh_addr;
                    g_section_map[g_section_map_count].loaded_addr = (uint8_t*)code_mem + code_offset;
                    g_section_map[g_section_map_count].size = shdr->sh_size;
                    ESP_LOGI(TAG, "  Map: ELF 0x%08lX -> RAM %p (%s, %lu bytes)",
                             shdr->sh_addr, (uint8_t*)code_mem + code_offset, section_name, shdr->sh_size);
                    g_section_map_count++;
                }
                
                code_offset += shdr->sh_size;
            }
        }
        
        ESP_LOGI(TAG, "✓ Code loaded at %p (%zu bytes)", app->code_segment, app->code_size);
    }
    
    // Allocate RAM for data sections
    if (app->data_size > 0) {
        app->data_segment = heap_caps_malloc(app->data_size, MALLOC_CAP_8BIT);
        if (!app->data_segment) {
            ESP_LOGE(TAG, "Failed to allocate %zu bytes for data", app->data_size);
            if (app->code_segment) free(app->code_segment);
            return ESP_ERR_NO_MEM;
        }
        
        // Copy data sections and record mappings
        size_t data_offset = 0;
        for (int i = 0; i < ehdr->e_shnum; i++) {
            elf32_shdr_t *shdr = &shdr_table[i];
            const char *section_name = shstrtab + shdr->sh_name;
            
            if ((shdr->sh_flags & SHF_ALLOC) && 
                !(shdr->sh_flags & SHF_EXECINSTR) &&
                (shdr->sh_type != SHT_NOBITS)) {
                
                void *dest = (uint8_t*)app->data_segment + data_offset;
                memcpy(dest, elf_data + shdr->sh_offset, shdr->sh_size);
                
                // Record section mapping
                if (g_section_map_count < 32) {
                    g_section_map[g_section_map_count].elf_addr = shdr->sh_addr;
                    g_section_map[g_section_map_count].loaded_addr = dest;
                    g_section_map[g_section_map_count].size = shdr->sh_size;
                    ESP_LOGI(TAG, "  Map: ELF 0x%08lX -> RAM %p (%s, %lu bytes)",
                             shdr->sh_addr, dest, section_name, shdr->sh_size);
                    g_section_map_count++;
                }
                
                data_offset += shdr->sh_size;
            }
        }
        
        ESP_LOGI(TAG, "✓ Data loaded at %p (%zu bytes)", app->data_segment, app->data_size);
    }
    
    // Allocate BSS (zero-initialized)
    if (app->bss_size > 0) {
        app->bss_segment = heap_caps_malloc(app->bss_size, MALLOC_CAP_8BIT);
        if (!app->bss_segment) {
            ESP_LOGE(TAG, "Failed to allocate %zu bytes for BSS", app->bss_size);
            if (app->data_segment) free(app->data_segment);
            if (app->code_segment) free(app->code_segment);
            return ESP_ERR_NO_MEM;
        }
        memset(app->bss_segment, 0, app->bss_size);
        
        // Record BSS section mappings
        size_t bss_offset = 0;
        for (int i = 0; i < ehdr->e_shnum; i++) {
            elf32_shdr_t *shdr = &shdr_table[i];
            const char *section_name = shstrtab + shdr->sh_name;
            
            if ((shdr->sh_flags & SHF_ALLOC) && (shdr->sh_type == SHT_NOBITS)) {
                void *dest = (uint8_t*)app->bss_segment + bss_offset;
                
                // Record section mapping
                if (g_section_map_count < 32) {
                    g_section_map[g_section_map_count].elf_addr = shdr->sh_addr;
                    g_section_map[g_section_map_count].loaded_addr = dest;
                    g_section_map[g_section_map_count].size = shdr->sh_size;
                    ESP_LOGI(TAG, "  Map: ELF 0x%08lX -> RAM %p (%s, %lu bytes)",
                             shdr->sh_addr, dest, section_name, shdr->sh_size);
                    g_section_map_count++;
                }
                
                bss_offset += shdr->sh_size;
            }
        }
        
        ESP_LOGI(TAG, "✓ BSS allocated at %p (%zu bytes)", app->bss_segment, app->bss_size);
    }
    
    ESP_LOGI(TAG, "Total section mappings: %d", g_section_map_count);
    
    // CRITICAL: Synchronize caches after loading code
    // This ensures CPU sees the newly loaded code
    #ifdef CONFIG_IDF_TARGET_ESP32S3
    if (app->code_segment) {
        extern void Cache_WriteBack_All(void);
        extern void Cache_Invalidate_ICache_All(void);
        Cache_WriteBack_All();
        Cache_Invalidate_ICache_All();
        ESP_LOGI(TAG, "✓ Caches synchronized after section load");
    }
    #endif
    
    return ESP_OK;
}

/**
 * @brief Load ELF sections into memory
 */
static esp_err_t load_sections(const void *binary, loaded_app_t *app)
{
    elf32_ehdr_t *ehdr = (elf32_ehdr_t*)binary;
    elf32_shdr_t *shdr_table = (elf32_shdr_t*)((uint8_t*)binary + ehdr->e_shoff);
    
    // Get section header string table
    elf32_shdr_t *shstrtab = &shdr_table[ehdr->e_shstrndx];
    const char *shstrtab_data = (const char*)((uint8_t*)binary + shstrtab->sh_offset);
    
    size_t total_loadable_size = 0;
    
    // First pass: calculate total memory needed
    for (int i = 0; i < ehdr->e_shnum; i++) {
        elf32_shdr_t *shdr = &shdr_table[i];
        
        // Only allocate for sections that need to be in memory
        if (!(shdr->sh_flags & SHF_ALLOC)) {
            continue;
        }
        
        const char *section_name = shstrtab_data + shdr->sh_name;
        
        if (shdr->sh_type == SHT_PROGBITS) {
            if (shdr->sh_flags & SHF_EXECINSTR) {
                // Code section
                app->code_size += shdr->sh_size;
                ESP_LOGI(TAG, "  Code section: %s size=%lu", section_name, shdr->sh_size);
            } else {
                // Data section
                app->data_size += shdr->sh_size;
                ESP_LOGI(TAG, "  Data section: %s size=%lu", section_name, shdr->sh_size);
            }
        } else if (shdr->sh_type == SHT_NOBITS) {
            // BSS section (uninitialized data)
            app->bss_size += shdr->sh_size;
            ESP_LOGI(TAG, "  BSS section:  %s size=%lu", section_name, shdr->sh_size);
        }
    }
    
    ESP_LOGI(TAG, "Memory required:");
    ESP_LOGI(TAG, "  Code: %zu bytes", app->code_size);
    ESP_LOGI(TAG, "  Data: %zu bytes", app->data_size);
    ESP_LOGI(TAG, "  BSS:  %zu bytes", app->bss_size);
    
    // Allocate memory for code segment
    if (app->code_size > 0) {
        // Try IRAM first for small apps (better performance)
        app->code_segment = heap_caps_malloc(app->code_size, MALLOC_CAP_EXEC | MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT);
        if (app->code_segment) {
            ESP_LOGI(TAG, "✓ Allocated code segment at %p (IRAM)", app->code_segment);
        } else {
            // IRAM full, use PSRAM (requires CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y)
            ESP_LOGW(TAG, "IRAM full, using PSRAM for code segment");
            app->code_segment = heap_caps_malloc(app->code_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!app->code_segment) {
                ESP_LOGE(TAG, "Failed to allocate code segment (%zu bytes) in PSRAM", app->code_size);
                ESP_LOGE(TAG, "Solutions:");
                ESP_LOGE(TAG, "  1. Enable CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y in sdkconfig");
                ESP_LOGE(TAG, "  2. Reduce app code size");
                ESP_LOGE(TAG, "  3. Free up IRAM");
                return ESP_ERR_NO_MEM;
            }
            ESP_LOGI(TAG, "✓ Allocated code segment at %p (PSRAM)", app->code_segment);
        }
        app->code_in_flash = false;
    }
    
    if (app->data_size > 0) {
        app->data_segment = APP_MALLOC(app->data_size);
        if (!app->data_segment) {
            ESP_LOGE(TAG, "Failed to allocate data segment (%zu bytes)", app->data_size);
            if (app->code_segment) free(app->code_segment);
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "✓ Allocated data segment at %p", app->data_segment);
    }
    
    if (app->bss_size > 0) {
        app->bss_segment = APP_MALLOC(app->bss_size);
        if (!app->bss_segment) {
            ESP_LOGE(TAG, "Failed to allocate BSS segment (%zu bytes)", app->bss_size);
            if (app->code_segment) free(app->code_segment);
            if (app->data_segment) free(app->data_segment);
            return ESP_ERR_NO_MEM;
        }
        memset(app->bss_segment, 0, app->bss_size);
        ESP_LOGI(TAG, "✓ Allocated BSS segment at %p", app->bss_segment);
    }
    
    // Second pass: copy section data
    size_t code_offset = 0, data_offset = 0, bss_offset = 0;
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        elf32_shdr_t *shdr = &shdr_table[i];
        
        if (!(shdr->sh_flags & SHF_ALLOC)) {
            continue;
        }
        
        if (shdr->sh_type == SHT_PROGBITS) {
            const void *src = (const uint8_t*)binary + shdr->sh_offset;
            
            if (shdr->sh_flags & SHF_EXECINSTR) {
                // Copy code
                memcpy((uint8_t*)app->code_segment + code_offset, src, shdr->sh_size);
                code_offset += shdr->sh_size;
            } else {
                // Copy data
                memcpy((uint8_t*)app->data_segment + data_offset, src, shdr->sh_size);
                data_offset += shdr->sh_size;
            }
        }
    }
    
    // Set entry point (adjust if needed based on ELF entry)
    app->entry_point = app->code_segment;
    
    return ESP_OK;
}

/**
 * @brief Convert I-cache address to D-cache address for PSRAM writing
 * When code is in PSRAM I-cache (0x42...), we need D-cache addr (0x3C...) to write
 */
static void* icache_to_dcache(void *icache_addr)
{
    uintptr_t addr = (uintptr_t)icache_addr;
    
    // Check if it's in I-cache PSRAM range
    if (addr >= 0x42000000 && addr < 0x44000000) {
        // Convert to D-cache address
        uint32_t offset = addr - 0x42000000;
        return (void*)(0x3C000000 + offset);
    }
    
    // Not PSRAM or already D-cache address
    return icache_addr;
}

/**
 * @brief Apply relocations to loaded app
 */
esp_err_t app_loader_apply_relocations(loaded_app_t *app, const void *elf_data)
{
    elf32_ehdr_t *ehdr = (elf32_ehdr_t*)elf_data;
    elf32_shdr_t *shdr_table = (elf32_shdr_t*)((uint8_t*)elf_data + ehdr->e_shoff);
    
    ESP_LOGI(TAG, "Applying relocations...");
    
    // Find symbol table for resolving external symbols
    elf32_shdr_t *dynsym_shdr = NULL;
    elf32_shdr_t *dynstr_shdr = NULL;
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr_table[i].sh_type == SHT_DYNSYM) {
            dynsym_shdr = &shdr_table[i];
        } else if (shdr_table[i].sh_type == SHT_STRTAB && i != ehdr->e_shstrndx) {
            if (dynstr_shdr == NULL) {
                dynstr_shdr = &shdr_table[i];
            }
        }
    }
    
    elf32_sym_t *symtab = NULL;
    const char *strtab = NULL;
    if (dynsym_shdr && dynstr_shdr) {
        symtab = (elf32_sym_t*)((uint8_t*)elf_data + dynsym_shdr->sh_offset);
        strtab = (const char*)((uint8_t*)elf_data + dynstr_shdr->sh_offset);
    }
    
    int reloc_count = 0;
    int unresolved_count = 0;
    
    // Find relocation sections
    for (int i = 0; i < ehdr->e_shnum; i++) {
        elf32_shdr_t *shdr = &shdr_table[i];
        
        if (shdr->sh_type != SHT_RELA) {
            continue;
        }
        
        // Get the section being relocated
        elf32_shdr_t *target_shdr = &shdr_table[shdr->sh_info];
        
        // Get relocation entries
        size_t num_relocs = shdr->sh_size / sizeof(elf32_rela_t);
        elf32_rela_t *relocs = (elf32_rela_t*)((uint8_t*)elf_data + shdr->sh_offset);
        
        ESP_LOGI(TAG, "  Section %d: %zu relocations", i, num_relocs);
        
        for (size_t j = 0; j < num_relocs; j++) {
            elf32_rela_t *rel = &relocs[j];
            uint32_t type = rel->r_info & 0xFF;
            uint32_t sym_idx = rel->r_info >> 8;
            
            // Calculate relocation target address
            // rel->r_offset contains the ABSOLUTE ELF virtual address to patch
            void *reloc_addr = map_elf_addr_to_loaded(rel->r_offset);
            if (!reloc_addr) {
                ESP_LOGW(TAG, "Could not map relocation offset 0x%lX", rel->r_offset);
                continue;
            }
            
            // CRITICAL: Skip relocations to flash-mapped code (XIP is read-only)
            // Flash mapped addresses are typically 0x3C800000+ or 0x42xxxxxx
            uintptr_t addr_val = (uintptr_t)reloc_addr;
            if ((addr_val >= 0x3C000000 && addr_val < 0x3D000000) ||  // Flash mmap region
                (addr_val >= 0x42000000 && addr_val < 0x44000000)) {   // I-cache PSRAM
                if (app->code_in_flash) {
                    // XIP mode: code in flash is position-independent, no relocations needed
                    reloc_count++;
                    continue;
                }
            }
            
            // CRITICAL: If reloc_addr is in I-cache PSRAM, convert to D-cache for writing
            reloc_addr = icache_to_dcache(reloc_addr);
            
            // Get symbol if needed
            const char *sym_name = NULL;
            elf32_sym_t *sym = NULL;
            if (sym_idx != 0 && symtab && strtab) {
                sym = &symtab[sym_idx];
                if (sym->st_name != 0) {
                    sym_name = strtab + sym->st_name;
                }
            }
            
            // Apply relocation based on type
            switch (type) {
                case R_XTENSA_NONE:
                    // No relocation needed
                    break;
                    
                case R_XTENSA_32: {
                    // Absolute 32-bit relocation - S + A
                    // For PIC, addend is offset from base, not absolute address
                    uint32_t *loc = (uint32_t*)reloc_addr;
                    
                    // Try to map addend as an address first
                    void *target = map_elf_addr_to_loaded(rel->r_addend);
                    if (target) {
                        *loc = (uint32_t)target;
                    } else {
                        // Fallback: treat as offset from code base (common for PIC)
                        *loc = (uint32_t)app->code_segment + rel->r_addend;
                    }
                    reloc_count++;
                    break;
                }
                
                case R_XTENSA_RELATIVE: {
                    // Relative relocation - B + A (base + addend)
                    // For PIC, if addend is 0, the value to relocate is stored at the location
                    uint32_t *loc = (uint32_t*)reloc_addr;
                    uint32_t elf_va = rel->r_addend;
                    
                    // If addend is 0, read the ELF virtual address from the location
                    if (elf_va == 0) {
                        elf_va = *loc;
                    }
                    
                    // Map ELF virtual address to loaded RAM address
                    void *target = map_elf_addr_to_loaded(elf_va);
                    if (target) {
                        *loc = (uint32_t)target;
                        if (reloc_count < 5) {  // Log first few for debugging
                            ESP_LOGI(TAG, "    RELATIVE @ ELF 0x%08lX -> RAM %p (was ELF 0x%08lX)", 
                                    rel->r_offset, target, elf_va);
                        }
                    } else {
                        // Fallback: treat as offset from code base
                        *loc = (uint32_t)app->code_segment + elf_va;
                        if (reloc_count < 5) {
                            ESP_LOGI(TAG, "    RELATIVE @ ELF 0x%08lX -> RAM %p (fallback, was ELF 0x%08lX)", 
                                    rel->r_offset, (void*)(*loc), elf_va);
                        }
                    }
                    reloc_count++;
                    break;
                }
                
                case R_XTENSA_GLOB_DAT: {
                    // Global data relocation - S (symbol value)
                    uint32_t *loc = (uint32_t*)reloc_addr;
                    
                    // For external symbols, resolve them
                    if (sym && sym->st_shndx == 0 && sym_name) {
                        // External symbol - resolve it
                        void *resolved = resolve_external_symbol(sym_name);
                        if (resolved) {
                            *loc = (uint32_t)resolved;
                            ESP_LOGD(TAG, "    GLOB_DAT %s -> %p", sym_name, resolved);
                        } else {
                            ESP_LOGW(TAG, "    Unresolved symbol: %s", sym_name);
                            *loc = 0;
                            unresolved_count++;
                        }
                    } else {
                        // Internal symbol - use addend
                        void *target = map_elf_addr_to_loaded(rel->r_addend);
                        if (target) {
                            *loc = (uint32_t)target;
                        } else {
                            *loc = (uint32_t)app->code_segment + rel->r_addend;
                        }
                    }
                    reloc_count++;
                    break;
                }
                
                case R_XTENSA_JMP_SLOT: {
                    // Jump slot relocation for PLT - S (symbol value)
                    // This is used for function calls through PLT/GOT
                    uint32_t *loc = (uint32_t*)reloc_addr;
                    
                    if (sym && sym_name) {
                        void *resolved = resolve_external_symbol(sym_name);
                        if (resolved) {
                            *loc = (uint32_t)resolved;
                            ESP_LOGD(TAG, "    JMP_SLOT %s -> %p", sym_name, resolved);
                        } else {
                            ESP_LOGW(TAG, "    Unresolved PLT: %s", sym_name);
                            *loc = 0;
                            unresolved_count++;
                        }
                    } else {
                        // No symbol - use addend
                        void *target = map_elf_addr_to_loaded(rel->r_addend);
                        if (target) {
                            *loc = (uint32_t)target;
                        } else {
                            *loc = (uint32_t)app->code_segment + rel->r_addend;
                        }
                    }
                    reloc_count++;
                    break;
                }
                
                case R_XTENSA_RTLD: {
                    // Runtime loader relocation
                    uint32_t *loc = (uint32_t*)reloc_addr;
                    
                    void *target = map_elf_addr_to_loaded(rel->r_addend);
                    if (target) {
                        *loc = (uint32_t)target;
                    } else {
                        *loc = (uint32_t)app->code_segment + rel->r_addend;
                    }
                    reloc_count++;
                    break;
                }
                
                case R_XTENSA_SLOT0_OP: {
                    // Xtensa instruction relocation (common for PIC)
                    // This is simplified - full implementation would decode instruction
                    reloc_count++;
                    break;
                }
                
                case R_XTENSA_ASM_EXPAND: {
                    // Assembly expansion directive - no action needed
                    // This is just a marker for the assembler
                    reloc_count++;
                    break;
                }
                
                default:
                    ESP_LOGW(TAG, "Unknown relocation type %lu at offset 0x%lX", type, rel->r_offset);
                    break;
            }
        }
    }
    
    ESP_LOGI(TAG, "✓ Applied %d relocations", reloc_count);
    
    if (unresolved_count > 0) {
        ESP_LOGW(TAG, "⚠  %d symbols could not be resolved", unresolved_count);
        return ESP_ERR_NOT_FOUND;
    }
    
    // CRITICAL: Synchronize caches after relocations
    // Relocations modify code/data, so we must flush caches
    #ifdef CONFIG_IDF_TARGET_ESP32S3
    extern void Cache_WriteBack_All(void);
    extern void Cache_Invalidate_ICache_All(void);
    Cache_WriteBack_All();
    Cache_Invalidate_ICache_All();
    ESP_LOGI(TAG, "✓ Caches synchronized after relocations");
    #endif
    
    return ESP_OK;
}

/**
 * @brief Map ELF virtual address to loaded RAM address
 */
static void* map_elf_addr_to_loaded(uint32_t elf_addr)
{
    for (int i = 0; i < g_section_map_count; i++) {
        uint32_t section_start = g_section_map[i].elf_addr;
        uint32_t section_end = section_start + g_section_map[i].size;
        
        if (elf_addr >= section_start && elf_addr < section_end) {
            // Address is within this section
            uint32_t offset = elf_addr - section_start;
            return (uint8_t*)g_section_map[i].loaded_addr + offset;
        }
    }
    
    return NULL;
}

/**
 * @brief Resolve external symbol names to function pointers
 * 
 * Maps symbol names from the app ELF to actual function addresses in firmware.
 * This is critical for dynamic linking - apps call these via PLT/GOT.
 */
static void* resolve_external_symbol(const char *sym_name)
{
    if (!sym_name) return NULL;
    
    // Standard library functions
    if (strcmp(sym_name, "malloc") == 0) return (void*)api_malloc_prefer_psram;
    if (strcmp(sym_name, "free") == 0) return (void*)free;
    if (strcmp(sym_name, "calloc") == 0) return (void*)api_calloc_prefer_psram;
    if (strcmp(sym_name, "realloc") == 0) return (void*)api_realloc_prefer_psram;
    if (strcmp(sym_name, "memset") == 0) return (void*)memset;
    if (strcmp(sym_name, "memcpy") == 0) return (void*)memcpy;
    if (strcmp(sym_name, "strlen") == 0) return (void*)strlen;
    if (strcmp(sym_name, "strcmp") == 0) return (void*)strcmp;
    if (strcmp(sym_name, "strncpy") == 0) return (void*)strncpy;
    
    // ESP logging
    if (strcmp(sym_name, "esp_log_write") == 0) return (void*)esp_log_write;
    if (strcmp(sym_name, "esp_log_timestamp") == 0) return (void*)esp_log_timestamp;
    if (strcmp(sym_name, "esp_log") == 0) return (void*)esp_log_write;
    
    // Memory allocation with caps
    if (strcmp(sym_name, "heap_caps_malloc") == 0) return (void*)heap_caps_malloc;
    if (strcmp(sym_name, "heap_caps_free") == 0) return (void*)heap_caps_free;
    
    // FreeRTOS
    if (strcmp(sym_name, "vTaskDelay") == 0) return (void*)vTaskDelay;
    if (strcmp(sym_name, "xTaskGetTickCount") == 0) return (void*)xTaskGetTickCount;
    
    // System services (these should be called through app_context, but just in case)
    if (strcmp(sym_name, "system_service_register") == 0) return (void*)system_service_register;
    if (strcmp(sym_name, "system_service_set_state") == 0) return (void*)system_service_set_state;
    if (strcmp(sym_name, "system_service_heartbeat") == 0) return (void*)system_service_heartbeat;
    if (strcmp(sym_name, "system_event_post") == 0) return (void*)system_event_post;
    if (strcmp(sym_name, "system_event_subscribe") == 0) return (void*)system_event_subscribe;
    if (strcmp(sym_name, "system_event_register_type") == 0) return (void*)system_event_register_type;
    
    // Memory utility (custom)
    if (strcmp(sym_name, "memory_log_usage") == 0) return (void*)memory_log_usage;
    
    ESP_LOGW(TAG, "Unresolved external symbol: %s", sym_name);
    return NULL;
}

/**
 * @brief Resolve external symbols
 */
esp_err_t app_loader_resolve_symbols(loaded_app_t *app, const void *elf_data)
{
    elf32_ehdr_t *ehdr = (elf32_ehdr_t*)elf_data;
    elf32_shdr_t *shdr_table = (elf32_shdr_t*)((uint8_t*)elf_data + ehdr->e_shoff);
    
    ESP_LOGI(TAG, "Resolving symbols...");
    
    // Set the API table pointer for system calls
    app->api_table = &g_system_api_table;
    
    // Find .dynsym and .dynstr sections
    elf32_shdr_t *dynsym_shdr = NULL;
    elf32_shdr_t *dynstr_shdr = NULL;
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdr_table[i].sh_type == SHT_DYNSYM) {
            dynsym_shdr = &shdr_table[i];
        } else if (shdr_table[i].sh_type == SHT_STRTAB && i != ehdr->e_shstrndx) {
            // Dynamic string table (not section header string table)
            if (dynstr_shdr == NULL) {
                dynstr_shdr = &shdr_table[i];
            }
        }
    }
    
    if (dynsym_shdr && dynstr_shdr) {
        elf32_sym_t *symtab = (elf32_sym_t*)((uint8_t*)elf_data + dynsym_shdr->sh_offset);
        const char *strtab = (const char*)((uint8_t*)elf_data + dynstr_shdr->sh_offset);
        size_t num_syms = dynsym_shdr->sh_size / sizeof(elf32_sym_t);
        
        ESP_LOGI(TAG, "Found %zu dynamic symbols", num_syms);
        
        // Log external symbols that need resolution
        for (size_t i = 0; i < num_syms; i++) {
            elf32_sym_t *sym = &symtab[i];
            if (sym->st_shndx == 0 && sym->st_name != 0) {
                // Undefined symbol (external)
                const char *sym_name = strtab + sym->st_name;
                ESP_LOGD(TAG, "  External symbol: %s", sym_name);
            }
        }
    }
    
    // Try to find entry function from symbol table
    // Look for symbols ending in "_app_entry", "_app_exit", and "_app_manifest"
    void *entry_from_symbol = NULL;
    void *exit_from_symbol = NULL;
    void *manifest_from_symbol = NULL;
    
    if (dynsym_shdr && dynstr_shdr) {
        elf32_sym_t *symtab = (elf32_sym_t*)((uint8_t*)elf_data + dynsym_shdr->sh_offset);
        const char *strtab = (const char*)((uint8_t*)elf_data + dynstr_shdr->sh_offset);
        size_t num_syms = dynsym_shdr->sh_size / sizeof(elf32_sym_t);
        
        for (size_t i = 0; i < num_syms; i++) {
            elf32_sym_t *sym = &symtab[i];
            if (sym->st_name == 0) continue;
            
            const char *sym_name = strtab + sym->st_name;
            size_t name_len = strlen(sym_name);
            
            // Look for *_app_entry symbol
            if (name_len > 10 && strcmp(sym_name + name_len - 10, "_app_entry") == 0) {
                if (sym->st_shndx != 0) {  // Defined symbol
                    void *addr = map_elf_addr_to_loaded(sym->st_value);
                    if (addr) {
                        entry_from_symbol = addr;
                        ESP_LOGI(TAG, "Found entry symbol '%s' at %p (ELF: 0x%08lX)", 
                                sym_name, addr, sym->st_value);
                    }
                }
            }
            
            // Look for *_app_exit symbol
            if (name_len > 9 && strcmp(sym_name + name_len - 9, "_app_exit") == 0) {
                if (sym->st_shndx != 0) {  // Defined symbol
                    void *addr = map_elf_addr_to_loaded(sym->st_value);
                    if (addr) {
                        exit_from_symbol = addr;
                        ESP_LOGI(TAG, "Found exit symbol '%s' at %p (ELF: 0x%08lX)", 
                                sym_name, addr, sym->st_value);
                    }
                }
            }
            
            // Look for *_app_manifest symbol
            if (name_len > 13 && strcmp(sym_name + name_len - 13, "_app_manifest") == 0) {
                if (sym->st_shndx != 0) {  // Defined symbol
                    void *addr = map_elf_addr_to_loaded(sym->st_value);
                    if (addr) {
                        manifest_from_symbol = addr;
                        ESP_LOGI(TAG, "Found manifest symbol '%s' at %p (ELF: 0x%08lX)", 
                                sym_name, addr, sym->st_value);
                    }
                }
            }
        }
    }
    
    // Set entry point - prefer symbol, then e_entry, then code segment start
    if (entry_from_symbol) {
        app->entry_point = entry_from_symbol;
    } else if (ehdr->e_entry != 0) {
        app->entry_point = map_elf_addr_to_loaded(ehdr->e_entry);
        if (!app->entry_point) {
            ESP_LOGW(TAG, "Failed to map e_entry 0x%08lX, using code segment start", ehdr->e_entry);
            app->entry_point = app->code_segment;
        }
    } else {
        ESP_LOGW(TAG, "No entry point found in ELF, using code segment start");
        app->entry_point = app->code_segment;
    }
    
    // Set exit point and manifest from symbols if found
    app->exit_point = exit_from_symbol;
    app->manifest_ptr = manifest_from_symbol;
    
    ESP_LOGI(TAG, "✓ Symbols resolved");
    ESP_LOGI(TAG, "  Entry point: %p (ELF e_entry=0x%08lX)", app->entry_point, ehdr->e_entry);
    
    return ESP_OK;
}

/**
 * @brief Load a dynamic app from binary
 */
esp_err_t app_loader_load_binary(const void *binary, size_t size, loaded_app_t *out_app)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Loading app binary (%zu bytes)...", size);
    
    memset(out_app, 0, sizeof(loaded_app_t));
    
    // Parse and validate ELF header
    elf32_ehdr_t ehdr;
    ret = parse_elf_header(binary, size, &ehdr);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Load sections into memory
    ret = load_sections(binary, out_app);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load sections");
        return ret;
    }
    
    // Apply relocations
    ret = app_loader_apply_relocations(out_app, binary);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply relocations");
        app_loader_unload(out_app);
        return ret;
    }
    
    // Resolve symbols
    ret = app_loader_resolve_symbols(out_app, binary);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to resolve symbols");
        app_loader_unload(out_app);
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ App loaded successfully");
    ESP_LOGI(TAG, "  Entry point: %p", out_app->entry_point);
    
    return ESP_OK;
}

/**
 * @brief Load app binary using hybrid approach
 * 
 * Code is copied from ELF to executable RAM (IRAM), data/bss in RAM
 */
static esp_err_t app_loader_load_binary_hybrid(const uint8_t *elf_data, size_t size,
                                                 void *code_flash_ptr,
                                                 spi_flash_mmap_handle_t mmap_handle,
                                                 loaded_app_t *out_app)
{
    esp_err_t ret;
    
    // Initialize output structure
    memset(out_app, 0, sizeof(loaded_app_t));
    out_app->mmap_handle = mmap_handle;  // Store for later unmapping
    
    // Parse and validate ELF header
    elf32_ehdr_t ehdr;
    ret = parse_elf_header(elf_data, size, &ehdr);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // XIP MODE: Execute code directly from flash, only load data to RAM
    if (code_flash_ptr != NULL) {
        ESP_LOGI(TAG, "Using XIP (Execute In Place) from flash at %p", code_flash_ptr);
        
        // Code stays in flash (XIP)
        out_app->code_segment = code_flash_ptr;
        out_app->code_in_flash = true;
        
        // Calculate code size from ELF sections
        elf32_shdr_t *shdr_table = (elf32_shdr_t*)((uint8_t*)elf_data + ehdr.e_shoff);
        const char *shstrtab = (const char*)((uint8_t*)elf_data + shdr_table[ehdr.e_shstrndx].sh_offset);
        
        for (int i = 0; i < ehdr.e_shnum; i++) {
            elf32_shdr_t *shdr = &shdr_table[i];
            if ((shdr->sh_flags & SHF_ALLOC) && (shdr->sh_flags & SHF_EXECINSTR)) {
                out_app->code_size += shdr->sh_size;
            }
        }
        
        ESP_LOGI(TAG, "Code in flash: %zu bytes at %p (XIP)", out_app->code_size, code_flash_ptr);
        
        // Load only data sections to RAM - no code copying needed!
        ret = load_data_sections_only(elf_data, out_app);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load data sections");
            return ret;
        }
        
    } else {
        // Fallback: Load sections - code copied to executable RAM, data/bss in RAM
        ret = load_sections_hybrid(elf_data, out_app);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load sections");
            return ret;
        }
    }
    
    // Apply relocations (to all sections now in RAM/PSRAM)
    ret = app_loader_apply_relocations(out_app, elf_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply relocations");
        // Free PSRAM: use psram_data_buf if it exists (not code_segment which is I-cache)
        if (out_app->psram_data_buf) free(out_app->psram_data_buf);
        else if (out_app->code_segment) free(out_app->code_segment);
        if (out_app->data_segment) free(out_app->data_segment);
        if (out_app->bss_segment) free(out_app->bss_segment);
        return ret;
    }
    
    // Resolve symbols
    ret = app_loader_resolve_symbols(out_app, elf_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to resolve symbols");
        if (out_app->psram_data_buf) free(out_app->psram_data_buf);
        else if (out_app->code_segment) free(out_app->code_segment);
        if (out_app->data_segment) free(out_app->data_segment);
        if (out_app->bss_segment) free(out_app->bss_segment);
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ App loaded successfully (IRAM execution)");
    ESP_LOGI(TAG, "  Code: %p (RAM)", out_app->code_segment);
    ESP_LOGI(TAG, "  Entry: %p", out_app->entry_point);
    
    return ESP_OK;
}

/**
 * @brief Load app from flash partition (Production approach)
 * 
 * Reads ELF from flash, copies code to executable RAM (IRAM)
 */
esp_err_t app_loader_load_from_partition(const char *partition_label, size_t offset, loaded_app_t *out_app)
{
    ESP_LOGI(TAG, "Loading app from partition '%s' at offset %zu", partition_label, offset);
    
    // Find partition
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        partition_label
    );
    
    if (!partition) {
        ESP_LOGE(TAG, "Partition '%s' not found", partition_label);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Found partition: size=%lu, address=0x%lX", partition->size, partition->address);
    
    // Read the ELF from flash to a temporary buffer for parsing
    // We can't use XIP on ESP32-S3 because I-cache is execute-only (can't read literals)
    // and D-cache is data-only (can't execute). So we copy code to executable RAM.
    
    // Read ELF header to determine size
    elf32_ehdr_t ehdr;
    esp_err_t ret = esp_partition_read(partition, offset, &ehdr, sizeof(ehdr));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ELF header");
        return ret;
    }
    
    // Validate header
    ret = parse_elf_header(&ehdr, sizeof(ehdr), &ehdr);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Calculate full ELF size
    size_t elf_size = ehdr.e_shoff + (ehdr.e_shnum * ehdr.e_shentsize);
    ESP_LOGI(TAG, "Reading ELF (%zu bytes)...", elf_size);
    
    // Allocate temporary buffer to hold entire ELF
    uint8_t *elf_buffer = malloc(elf_size);
    if (!elf_buffer) {
        ESP_LOGE(TAG, "Failed to allocate ELF buffer (%zu bytes)", elf_size);
        return ESP_ERR_NO_MEM;
    }
    
    // Read complete ELF from flash
    ret = esp_partition_read(partition, offset, elf_buffer, elf_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ELF from partition");
        free(elf_buffer);
        return ret;
    }
    
    // Load app - code will be copied to executable RAM (IRAM or PSRAM)
    // Pass NULL for flash pointer since XIP doesn't work on ESP32-S3
    ret = app_loader_load_binary_hybrid(elf_buffer, elf_size, NULL, 0, out_app);
    
    // Free temporary ELF buffer
    free(elf_buffer);
    
    return ret;
}

/**
 * @brief Unload a dynamic app
 */
esp_err_t app_loader_unload(loaded_app_t *app)
{
    ESP_LOGI(TAG, "Unloading app...");
    
    // If code is in flash (XIP), unmap it
    if (app->code_in_flash && app->mmap_handle) {
        spi_flash_munmap(app->mmap_handle);
        app->mmap_handle = 0;
        app->code_segment = NULL;  // Don't free - it's mapped flash
    } else {
        // Code was in RAM - free the D-cache buffer if PSRAM, else code_segment
        if (app->psram_data_buf) {
            free(app->psram_data_buf);
            app->psram_data_buf = NULL;
        } else if (app->code_segment) {
            free(app->code_segment);
        }
        app->code_segment = NULL;
    }
    
    if (app->data_segment) {
        free(app->data_segment);
        app->data_segment = NULL;
    }
    
    if (app->bss_segment) {
        free(app->bss_segment);
        app->bss_segment = NULL;
    }
    
    memset(app, 0, sizeof(loaded_app_t));
    
    ESP_LOGI(TAG, "✓ App unloaded");
    return ESP_OK;
}
