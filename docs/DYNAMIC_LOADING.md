# Dynamic App Loading - Implementation Guide

## Overview

Kraken now supports **dynamic app loading** - apps can be compiled separately using the same source code and loaded at runtime from storage or network.

## How It Works

### Architecture

```
┌──────────────────────────────────────────┐
│         Kraken Firmware (kraken.bin)     │
├──────────────────────────────────────────┤
│  System Services (SRAM)                  │
│  ├─ Service Manager                      │
│  ├─ Event Bus                            │
│  ├─ Symbol Table ◄─────┐                 │
│  └─ App Loader         │                 │
│                        │                 │
│  Built-in Apps         │                 │
│  ├─ hello (optional)   │                 │
│  └─ goodbye (optional) │                 │
└────────────────────────┼─────────────────┘
                         │
                         │ Exports system_service APIs
                         │
┌────────────────────────▼─────────────────┐
│      Storage / Network                   │
├──────────────────────────────────────────┤
│  Dynamic Apps (PSRAM when loaded)        │
│  ├─ hello.bin  ◄── Uses same source!     │
│  ├─ goodbye.bin                          │
│  └─ custom.bin                           │
└──────────────────────────────────────────┘
```

### Key Features

✅ **Shared Source Code** - Apps use the same source as built-in apps  
✅ **System API Access** - Symbol table exports system_service APIs  
✅ **PSRAM Loading** - Apps loaded into external memory  
✅ **Runtime Loading** - Load apps from storage or HTTP  
✅ **Hot Swapping** - Update apps without reflashing firmware  

## Building Dynamic Apps

### Step 1: Build Main Firmware

```bash
cd /home/user/kraken/kraken
idf.py build
```

This builds `kraken.bin` with the system services and symbol table.

### Step 2: Build Dynamic Apps

```bash
# Build single app
python build_dynamic_app.py hello

# Build all apps
python build_dynamic_app.py --all
```

This creates `build/apps/hello.bin` with:
- App header (128 bytes)
- App code (extracted from object file)
- CRC32 checksum

### App Binary Format

```
┌────────────────────────────────────────┐
│  App Header (128 bytes)                │
│  ┌──────────────────────────────────┐  │
│  │ Magic:       0x4150504B ("APPK") │  │
│  │ Name:        "hello" (32 bytes)  │  │
│  │ Version:     "1.0.0" (16 bytes)  │  │
│  │ Author:      "..." (32 bytes)    │  │
│  │ Size:        1024 (4 bytes)      │  │
│  │ Entry Point: 0x0000 (4 bytes)    │  │
│  │ CRC32:       0x12345678          │  │
│  └──────────────────────────────────┘  │
├────────────────────────────────────────┤
│  App Code                              │
│  - Compiled from same source           │
│  - Uses system_service APIs            │
│  - Links to symbol table at runtime    │
└────────────────────────────────────────┘
```

## Symbol Table

The symbol table exports system APIs to dynamic apps:

### Exported Functions

```c
// Service Manager
service_manager_register()
service_manager_unregister()
service_manager_set_state()
service_manager_heartbeat()

// Event Bus
event_bus_post_event()
event_bus_subscribe()
event_bus_unsubscribe()
event_bus_register_event_type()

// Memory
malloc(), free(), calloc()
heap_caps_malloc(), heap_caps_free()

// Logging
esp_log_write()
```

### Usage in Apps

Apps use the same code as built-in apps:

```c
// hello_app.c - Same source for both built-in and dynamic!
#include "system_service/app_manager.h"
#include "system_service/memory_utils.h"
#include "esp_log.h"

static const char *TAG = "hello_app";

esp_err_t hello_app_entry(app_context_t *ctx) {
    ESP_LOGI(TAG, "Hello from dynamic app!");
    
    // Allocate in PSRAM
    uint8_t *data = APP_MALLOC(50000);
    
    // Use event bus
    system_event_type_t event_type;
    ctx->register_event_type("app.hello.custom", &event_type);
    
    // All APIs work the same!
    free(data);
    return ESP_OK;
}
```

## Loading Apps at Runtime

### From Storage

```c
// Load app from filesystem
app_info_t app_info;
esp_err_t ret = app_manager_load_from_storage("/storage/apps/hello.bin", &app_info);

if (ret == ESP_OK) {
    // Start the app
    app_manager_start_app("hello");
}
```

### From Network

```c
// Download and load app from URL
app_info_t app_info;
esp_err_t ret = app_manager_load_from_url("http://server.com/apps/hello.bin", &app_info);

if (ret == ESP_OK) {
    app_manager_start_app("hello");
}
```

## Uploading Apps to ESP32

### Method 1: Flash to Storage Partition

```bash
# Upload to storage partition (starts at offset in partitions.csv)
esptool.py --port /dev/ttyUSB0 write_flash 0x410000 build/apps/hello.bin
```

### Method 2: Copy to Filesystem

```bash
# If using FAT/SPIFFS
# 1. Mount partition
# 2. Copy file to /storage/apps/
# 3. Load at runtime
```

### Method 3: HTTP Download

```bash
# Serve apps via HTTP
python -m http.server 8080 --directory build/apps

# ESP32 downloads at runtime
app_manager_load_from_url("http://192.168.1.100:8080/hello.bin", &info);
```

## Memory Usage

### System (SRAM)
- System services: ~100-200 KB
- Symbol table: ~10 KB
- Event bus: ~20 KB
- Service manager: ~20 KB

### Apps (PSRAM)
- App code: Variable (1-100 KB typical)
- App data: Variable (can use all available PSRAM)
- Dynamically allocated

### Example
```
SRAM:  System = 150 KB, Free = 370 KB
PSRAM: Apps = 200 KB, Free = 7.9 MB
```

## Limitations & Current Status

### ✅ Working
- App header parsing and verification
- CRC32 validation
- PSRAM allocation for apps
- Symbol table for system APIs
- Loading from storage/network
- Same source code for built-in and dynamic apps

### ⚠️ Limitations
- **Not position-independent**: Apps compiled with absolute addresses
- **Basic relocation**: Entry point offset only
- **No ELF support**: Custom binary format
- **Limited debugging**: Harder to debug than built-in apps

### 🔧 Future Improvements
1. **Full PIC support**: Compile with `-fPIC`
2. **ELF format**: Use standard ELF with relocations
3. **Symbol versioning**: API compatibility checking
4. **App sandboxing**: Resource limits, permissions
5. **Hot reload**: Unload/reload apps without restart

## Example Workflow

### 1. Develop App

```c
// components/apps/myapp/myapp_app.c
#include "system_service/app_manager.h"
#include "system_service/memory_utils.h"

esp_err_t myapp_entry(app_context_t *ctx) {
    memory_log_usage("myapp");
    // Your app logic...
    return ESP_OK;
}

const app_manifest_t myapp_manifest = {
    .name = "myapp",
    .version = "1.0.0",
    .author = "Your Name",
    .entry = myapp_entry,
};
```

### 2. Test as Built-in

```bash
# Add to CMakeLists.txt and main.c
idf.py build flash monitor
# Test app works correctly
```

### 3. Build as Dynamic

```bash
# Build dynamic version
python build_dynamic_app.py myapp

# Creates build/apps/myapp.bin
```

### 4. Deploy

```bash
# Option A: Flash to storage
esptool.py write_flash 0x410000 build/apps/myapp.bin

# Option B: Copy to filesystem
# (mount and copy myapp.bin to /storage/apps/)

# Option C: Serve via HTTP
python -m http.server 8080 --directory build/apps
```

### 5. Load at Runtime

```c
// In your main code or via command
app_info_t info;

// From storage
app_manager_load_from_storage("/storage/apps/myapp.bin", &info);

// Or from network
app_manager_load_from_url("http://server/myapp.bin", &info);

// Start the app
app_manager_start_app("myapp");
```

## Debugging

### Check Symbol Table

```c
size_t count;
const symbol_entry_t *symbols = symbol_table_get_all(&count);

for (size_t i = 0; i < count; i++) {
    ESP_LOGI(TAG, "Symbol: %s @ %p", symbols[i].name, symbols[i].address);
}
```

### Verify App Binary

```c
// Load and verify without executing
app_header_t *header = (app_header_t*)binary_data;
app_verify_header(header, total_size);

uint32_t crc = app_calculate_crc32(code_data, header->size);
ESP_LOGI(TAG, "CRC: calculated=0x%08X, expected=0x%08X", crc, header->crc32);
```

### Memory Debugging

```c
// Before loading
memory_log_usage("before");

// Load app
app_manager_load_from_storage("/apps/hello.bin", &info);

// After loading
memory_log_usage("after");
```

## API Reference

### Symbol Table API

```c
// Initialize (called by system_service_init)
esp_err_t symbol_table_init(void);

// Register custom API
esp_err_t symbol_table_register(const char *name, void *address, symbol_type_t type);

// Lookup symbol
void* symbol_table_lookup(const char *name);

// Get all symbols
const symbol_entry_t* symbol_table_get_all(size_t *count);
```

### App Loader API

```c
// Load app binary
esp_err_t app_load_binary(const void *data, size_t size, app_info_t *info);

// Verify header
esp_err_t app_verify_header(const app_header_t *header, size_t total_size);

// Calculate CRC32
uint32_t app_calculate_crc32(const uint8_t *data, size_t size);
```

## Troubleshooting

### App Won't Load

```
Error: Invalid magic: 0x00000000
```
**Fix**: Binary file corrupted or wrong format. Rebuild with `build_dynamic_app.py`

### CRC Mismatch

```
Error: CRC32 mismatch
```
**Fix**: File corrupted during transfer. Re-upload the .bin file.

### Out of Memory

```
Error: Failed to allocate PSRAM
```
**Fix**: App too large or PSRAM full. Free other apps or reduce app size.

### Symbol Not Found

```
Warning: Symbol not found: my_function
```
**Fix**: Function not exported. Add to symbol table or use built-in APIs only.

## Best Practices

1. **Test Built-in First**: Always test apps as built-in before making dynamic
2. **Use PSRAM**: Apps automatically use PSRAM for data
3. **Check Return Values**: Always verify `app_manager_load_*` returns ESP_OK
4. **Monitor Memory**: Use `memory_log_usage()` to track allocation
5. **Version Apps**: Use semantic versioning in manifest
6. **Validate Checksums**: CRC32 ensures integrity during transfer

## Files Created

```
components/system/
├── include/system_service/
│   └── app_symbol_table.h          # Symbol table API
├── src/
│   ├── app_symbol_table.c          # Symbol table implementation
│   └── app_loader.c                # Updated with dynamic loading
│
build_dynamic_app.py                 # Build tool for dynamic apps
docs/
└── DYNAMIC_LOADING.md              # This file
```

## Next Steps

1. ✅ Build firmware: `idf.py build`
2. ✅ Build apps: `python build_dynamic_app.py --all`
3. ⚠️ Upload apps to storage
4. ⚠️ Test runtime loading
5. 🔧 Implement PIC for true position independence
