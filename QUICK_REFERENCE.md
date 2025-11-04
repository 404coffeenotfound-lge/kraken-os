# Kraken - Quick Reference Card

## Build Commands

```bash
# Build firmware (with built-in apps)
idf.py build

# Flash firmware
idf.py flash monitor

# Clean build
idf.py fullclean
idf.py build
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
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "system_service/memory_utils.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "myapp";

esp_err_t myapp_entry(app_context_t *ctx) {
    ESP_LOGI(TAG, "App started!");
    
    system_service_id_t service_id = ctx->service_id;
    ctx->set_state(service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    // Your app logic
    for (int i = 0; i < 10; i++) {
        ESP_LOGI(TAG, "Count: %d", i);
        ctx->heartbeat(service_id);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
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

## App Context API

```c
// Service management
ctx->set_state(service_id, SYSTEM_SERVICE_STATE_RUNNING);
ctx->heartbeat(service_id);

// Event registration
system_event_type_t my_event;
ctx->register_event_type("app.myapp.custom", &my_event);

// Event posting
ctx->post_event(service_id, my_event, &data, 
                sizeof(data), SYSTEM_EVENT_PRIORITY_NORMAL);

// Subscribe to event
ctx->subscribe_event(ctx->service_id, some_event, 
                     event_handler, user_data);
```

## App Registration & Startup

```c
// In main/kraken.c
#include "hello_app.h"
#include "goodbye_app.h"

// Register apps at startup
app_info_t *app_info = NULL;
app_manager_register_app(&hello_app_manifest, &app_info);
app_manager_register_app(&goodbye_app_manifest, &app_info);

// Start apps (delayed)
vTaskDelay(pdMS_TO_TICKS(2000));
app_manager_start_app("hello");
app_manager_start_app("goodbye");
```

## File Locations

```
kraken-os/
├── components/apps/          # App source code
├── components/system/        # System services
├── main/                     # Main firmware entry
├── docs/                     # Documentation
└── sdkconfig.defaults       # ESP-IDF config
```

## Common Event Types

```c
// System events (0-99)
COMMON_EVENT_SYSTEM_STARTUP     = 0
COMMON_EVENT_SYSTEM_SHUTDOWN    = 1

// Network events (100-199)
COMMON_EVENT_NETWORK_CONNECTED  = 100
COMMON_EVENT_NETWORK_DISCONNECTED = 101

// App events (200-299)
COMMON_EVENT_APP_STARTED        = 200
COMMON_EVENT_APP_STOPPED        = 201
```

## Documentation

| File | Purpose |
|------|---------|
| README.md | Main documentation |
| docs/ARCHITECTURE.md | System architecture |
| docs/APPS_VS_SERVICES.md | App/service integration |
| docs/MEMORY_MANAGEMENT.md | SRAM/PSRAM strategy |
| docs/INDEX.md | Documentation index |

## Troubleshooting

```bash
# Clean build
idf.py fullclean && idf.py build

# Monitor output
idf.py monitor

# Check memory in code
memory_log_usage("debug");

# Verify PSRAM
if (!memory_psram_available()) {
    ESP_LOGE(TAG, "No PSRAM!");
}
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
