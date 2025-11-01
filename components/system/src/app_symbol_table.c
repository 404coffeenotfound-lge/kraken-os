#include "system_service/app_symbol_table.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "symbol_table";

#define MAX_SYMBOLS 256

static symbol_entry_t g_symbols[MAX_SYMBOLS];
static size_t g_symbol_count = 0;

esp_err_t symbol_table_init(void)
{
    g_symbol_count = 0;
    
    ESP_LOGI(TAG, "Initializing symbol table for dynamic apps");
    
    // Export service manager APIs
    symbol_table_register("system_service_register", 
                         (void*)system_service_register, 
                         SYMBOL_TYPE_FUNCTION);
    symbol_table_register("system_service_unregister",
                         (void*)system_service_unregister,
                         SYMBOL_TYPE_FUNCTION);
    symbol_table_register("system_service_set_state",
                         (void*)system_service_set_state,
                         SYMBOL_TYPE_FUNCTION);
    symbol_table_register("system_service_heartbeat",
                         (void*)system_service_heartbeat,
                         SYMBOL_TYPE_FUNCTION);
    
    // Export event bus APIs
    symbol_table_register("system_event_post",
                         (void*)system_event_post,
                         SYMBOL_TYPE_FUNCTION);
    symbol_table_register("system_event_subscribe",
                         (void*)system_event_subscribe,
                         SYMBOL_TYPE_FUNCTION);
    symbol_table_register("system_event_unsubscribe",
                         (void*)system_event_unsubscribe,
                         SYMBOL_TYPE_FUNCTION);
    symbol_table_register("system_event_register_type",
                         (void*)system_event_register_type,
                         SYMBOL_TYPE_FUNCTION);
    
    // Export ESP-IDF APIs commonly used by apps
    symbol_table_register("esp_log_write",
                         (void*)esp_log_write,
                         SYMBOL_TYPE_FUNCTION);
    symbol_table_register("malloc",
                         (void*)malloc,
                         SYMBOL_TYPE_FUNCTION);
    symbol_table_register("free",
                         (void*)free,
                         SYMBOL_TYPE_FUNCTION);
    symbol_table_register("calloc",
                         (void*)calloc,
                         SYMBOL_TYPE_FUNCTION);
    symbol_table_register("heap_caps_malloc",
                         (void*)heap_caps_malloc,
                         SYMBOL_TYPE_FUNCTION);
    symbol_table_register("heap_caps_free",
                         (void*)heap_caps_free,
                         SYMBOL_TYPE_FUNCTION);
    
    ESP_LOGI(TAG, "✓ Exported %d symbols for dynamic apps", g_symbol_count);
    
    return ESP_OK;
}

esp_err_t symbol_table_register(const char *name, void *address, symbol_type_t type)
{
    if (name == NULL || address == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_symbol_count >= MAX_SYMBOLS) {
        ESP_LOGE(TAG, "Symbol table full");
        return ESP_ERR_NO_MEM;
    }
    
    // Check for duplicates
    for (size_t i = 0; i < g_symbol_count; i++) {
        if (strcmp(g_symbols[i].name, name) == 0) {
            ESP_LOGW(TAG, "Symbol '%s' already registered, updating", name);
            g_symbols[i].address = address;
            g_symbols[i].type = type;
            return ESP_OK;
        }
    }
    
    // Add new symbol
    g_symbols[g_symbol_count].name = name;
    g_symbols[g_symbol_count].address = address;
    g_symbols[g_symbol_count].type = type;
    g_symbol_count++;
    
    return ESP_OK;
}

void* symbol_table_lookup(const char *name)
{
    if (name == NULL) {
        return NULL;
    }
    
    for (size_t i = 0; i < g_symbol_count; i++) {
        if (strcmp(g_symbols[i].name, name) == 0) {
            return g_symbols[i].address;
        }
    }
    
    ESP_LOGW(TAG, "Symbol not found: %s", name);
    return NULL;
}

const symbol_entry_t* symbol_table_get_all(size_t *count)
{
    if (count) {
        *count = g_symbol_count;
    }
    return g_symbols;
}
