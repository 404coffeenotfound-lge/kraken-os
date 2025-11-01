#ifndef APP_SYMBOL_TABLE_H
#define APP_SYMBOL_TABLE_H

#include "esp_err.h"
#include "system_service/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Symbol table for exporting system_service APIs to dynamic apps
 * 
 * This allows apps to call system functions even when loaded dynamically.
 * Apps built separately can use the same source code as built-in apps.
 */

// Symbol types
typedef enum {
    SYMBOL_TYPE_FUNCTION,
    SYMBOL_TYPE_DATA,
} symbol_type_t;

// Symbol entry
typedef struct {
    const char *name;
    void *address;
    symbol_type_t type;
} symbol_entry_t;

/**
 * @brief Initialize symbol table with system APIs
 */
esp_err_t symbol_table_init(void);

/**
 * @brief Register a symbol (function or data)
 */
esp_err_t symbol_table_register(const char *name, void *address, symbol_type_t type);

/**
 * @brief Lookup a symbol by name
 */
void* symbol_table_lookup(const char *name);

/**
 * @brief Get all exported symbols
 */
const symbol_entry_t* symbol_table_get_all(size_t *count);

#ifdef __cplusplus
}
#endif

#endif // APP_SYMBOL_TABLE_H
