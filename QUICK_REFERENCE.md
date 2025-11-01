# Kraken - Quick Reference Card

## Build Commands

```bash
# Build firmware
idf.py build

# Build dynamic apps
python build_dynamic_app.py hello        # Single app
python build_dynamic_app.py --all        # All apps

# Flash firmware
idf.py flash monitor

# Upload app to storage
esptool.py write_flash 0x410000 build/apps/hello.bin
```

## Memory Macros

```c
// For Apps (PSRAM)
uint8_t *buf = APP_MALLOC(size);
APP_CALLOC(n, size);
APP_DATA_ATTR static uint8_t data[1024];

// For System Services (SRAM)
void *ctx = SYSTEM_MALLOC(size);
SYSTEM_CALLOC(n, size);

// DMA buffers (SRAM)
void *dma = DMA_MALLOC(size);

// Memory monitoring
memory_log_usage("tag");
memory_get_free_sram();
memory_get_free_psram();
```

## App Template

```c
#include "system_service/app_manager.h"
#include "system_service/memory_utils.h"
#include "esp_log.h"

static const char *TAG = "myapp";
APP_DATA_ATTR static uint8_t app_buffer[10240];

esp_err_t myapp_entry(app_context_t *ctx) {
    memory_log_usage(TAG);
    
    uint8_t *data = APP_MALLOC(50000);
    if (!data) return ESP_ERR_NO_MEM;
    
    // Your code here...
    
    free(data);
    return ESP_OK;
}

esp_err_t myapp_exit(app_context_t *ctx) {
    ESP_LOGI(TAG, "App exiting");
    return ESP_OK;
}

const app_manifest_t myapp_manifest = {
    .name = "myapp",
    .version = "1.0.0",
    .author = "Your Name",
    .entry = myapp_entry,
    .exit = myapp_exit,
};
```

## Runtime Loading

```c
// Load from storage
app_info_t info;
app_manager_load_from_storage("/storage/apps/hello.bin", &info);
app_manager_start_app("hello");

// Load from network
app_manager_load_from_url("http://server/hello.bin", &info);
app_manager_start_app("hello");
```

## Event Bus

```c
// Register event type
system_event_type_t my_event;
ctx->register_event_type("app.myapp.started", &my_event);

// Post event
ctx->post_event(ctx->service_id, my_event, &data, 
                sizeof(data), SYSTEM_EVENT_PRIORITY_NORMAL);

// Subscribe to event
ctx->subscribe_event(ctx->service_id, some_event, 
                     event_handler, user_data);
```

## File Locations

```
kraken/
├── components/apps/          # App source code
├── build/apps/              # Built app binaries
├── docs/                    # Documentation
├── build_dynamic_app.py     # Build tool
└── sdkconfig.defaults       # Config
```

## Partition Table

```
Offset    | Size | Description
----------|------|---------------------------
0x9000    | 20K  | NVS
0xe000    | 4K   | PHY Init
0x10000   | 4MB  | Firmware (kraken.bin)
0x410000  | 1MB  | Storage (apps, data)
```

## Documentation

| File | Purpose |
|------|---------|
| README.md | Main documentation |
| DYNAMIC_LOADING.md | Dynamic loader guide |
| MEMORY_MANAGEMENT.md | SRAM/PSRAM strategy |
| BUILDING_APPS.md | App building methods |
| QUICK_START.md | Getting started |

## Troubleshooting

```bash
# Clean build
idf.py fullclean && idf.py build

# Check memory
memory_log_usage("debug");

# Verify PSRAM
if (!memory_psram_available()) {
    ESP_LOGE(TAG, "No PSRAM!");
}

# Check app binary
hexdump -C build/apps/hello.bin | head -20
```

## Key Concepts

- **SRAM**: Fast internal memory for system services
- **PSRAM**: Large external memory for apps
- **Symbol Table**: Exports system APIs to dynamic apps
- **App Header**: 128-byte metadata in every app binary
- **Entry Point**: Offset to app's main function

## Memory Strategy

```
┌─────────────────────┐
│ SRAM (~520KB)       │
│ - System services   │
│ - WiFi/LWIP         │
│ - RTOS              │
└─────────────────────┘

┌─────────────────────┐
│ PSRAM (up to 32MB)  │
│ - App code          │
│ - App data          │
│ - Large buffers     │
└─────────────────────┘
```

## Common Errors

**`event->type` not found**
→ Use `event->event_type`

**Out of SRAM**
→ Use `APP_MALLOC()` instead of `malloc()`

**App won't load**
→ Check header magic: `hexdump -C app.bin | head -1`

**CRC mismatch**
→ Rebuild app: `python build_dynamic_app.py appname`
