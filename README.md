# Kraken - ESP32-S3 Application Framework

A modular firmware framework for ESP32-S3 featuring built-in apps, system services, and efficient memory management.

## Features

- 🚀 **Application System** - Built-in apps with lifecycle management
- 🔧 **System Services** - Modular service architecture (WiFi, Audio, Display, etc.)
- 💾 **Smart Memory Management** - Automatic SRAM/PSRAM allocation (system in SRAM, apps in PSRAM)
- 🔒 **Security** - Secure key-based service management
- 📦 **Event Bus** - Publish/subscribe event system for inter-component communication

## Quick Start

### Prerequisites

- ESP-IDF v5.5.1 or later
- ESP32-S3 development board with PSRAM
- Python 3.7+

### Build and Flash

```bash
# Clone and setup
git clone <repo-url> kraken-os
cd kraken-os

# Build
idf.py build

# Flash to ESP32-S3
idf.py flash monitor
```

## Project Structure

```
kraken-os/
├── main/                  # Main application entry point
├── components/
│   ├── system/           # Core system services
│   │   ├── src/         # Service manager, event bus, app manager
│   │   └── include/     # Public headers
│   └── apps/            # Built-in apps
│       ├── hello/       # Example hello app
│       └── goodbye/     # Example goodbye app
├── docs/                # Documentation
│   ├── ARCHITECTURE.md       # System architecture
│   ├── APPS_VS_SERVICES.md   # App/service integration
│   ├── MEMORY_MANAGEMENT.md  # SRAM/PSRAM strategy
│   └── INDEX.md              # Documentation index
└── sdkconfig.defaults   # ESP-IDF configuration
```

## Memory Management

Kraken implements intelligent memory allocation:

- **SRAM (Internal, ~520KB)**: System services, WiFi, RTOS, critical data
- **PSRAM (External, up to 32MB)**: App data, large buffers, GUI

### For App Developers

```c
#include "system_service/memory_utils.h"

// Allocate in PSRAM (for apps)
uint8_t *buffer = APP_MALLOC(50000);

// Static data in PSRAM
APP_DATA_ATTR static uint8_t large_buffer[100000];

// Monitor memory
memory_log_usage("my_app");
```

See [docs/MEMORY_MANAGEMENT.md](docs/MEMORY_MANAGEMENT.md) for details.

## Building Apps Separately

## Creating Your Own App

### 1. Create App Structure

```bash
mkdir -p components/apps/myapp
cd components/apps/myapp
```

### 2. Create App Source

```c
// myapp_app.c
#include "system_service/app_manager.h"
#include "system_service/service_manager.h"
#include "system_service/event_bus.h"
#include "system_service/memory_utils.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "myapp";

esp_err_t myapp_entry(app_context_t *ctx) {
    ESP_LOGI(TAG, "My app started!");
    
    system_service_id_t service_id = ctx->service_id;
    
    // Set state to running
    ctx->set_state(service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    // Register custom event
    system_event_type_t my_event;
    ctx->register_event_type("app.myapp.custom", &my_event);
    
    // Your app logic here...
    for (int i = 0; i < 5; i++) {
        ESP_LOGI(TAG, "Running iteration: %d", i);
        ctx->heartbeat(service_id);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    return ESP_OK;
}

esp_err_t myapp_exit(app_context_t *ctx) {
    ESP_LOGI(TAG, "My app exiting");
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

### 3. Create App Header

```c
// myapp_app.h
#ifndef MYAPP_APP_H
#define MYAPP_APP_H

#include "system_service/app_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const app_manifest_t myapp_manifest;

#ifdef __cplusplus
}
#endif

#endif // MYAPP_APP_H
```

### 4. Register App

Add to `components/apps/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS 
        "hello/hello_app.c"
        "goodbye/goodbye_app.c"
        "myapp/myapp_app.c"    # Add your app
    INCLUDE_DIRS 
        "hello"
        "goodbye"
        "myapp"                 # Add include path
    REQUIRES
        system
)
```

Register in `main/kraken.c`:
```c
#include "myapp_app.h"

void app_main(void) {
    // ... existing initialization ...
    
    app_info_t *app_info = NULL;
    app_manager_register_app(&myapp_manifest, &app_info);
    
    // Start app
    vTaskDelay(pdMS_TO_TICKS(2000));
    app_manager_start_app("myapp");
}
```

### 5. Build and Run

```bash
idf.py build flash monitor
```

## System Services

Create system services for reusable functionality:

```c
#include "system_service/service_manager.h"

esp_err_t my_service_init(system_secure_key_t key) {
    system_service_id_t service_id;
    
    // Register service (uses SRAM automatically)
    service_manager_register("my_service", NULL, &service_id);
    
    // Subscribe to events
    system_event_type_t event_type;
    event_bus_register_event_type("my_service.started", &event_type);
    
    return ESP_OK;
}
```

See [docs/APPS_VS_SERVICES.md](docs/APPS_VS_SERVICES.md) for guidance.

## Configuration

Key settings in `sdkconfig.defaults`:

```
# PSRAM - Explicit allocation for apps, SRAM for system
CONFIG_SPIRAM=y
CONFIG_SPIRAM_USE_CAPS_ALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384
```

Modify with:
```bash
idf.py menuconfig
```

## Documentation

| Document | Description |
|----------|-------------|
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | System architecture overview |
| [BUILDING_APPS.md](docs/BUILDING_APPS.md) | Building apps separately |
| [MEMORY_MANAGEMENT.md](docs/MEMORY_MANAGEMENT.md) | SRAM/PSRAM allocation strategy |
| [APPS_VS_SERVICES.md](docs/APPS_VS_SERVICES.md) | When to use apps vs services |
| [APP_SYSTEM_GUIDE.md](docs/APP_SYSTEM_GUIDE.md) | App development guide |
| [QUICK_START.md](docs/QUICK_START.md) | Getting started |

## API Reference

### App Context API

```c
typedef struct app_context {
    app_info_t *app_info;
    system_service_id_t service_id;
    
    // Service management
    esp_err_t (*register_service)(...);
    esp_err_t (*set_state)(...);
    esp_err_t (*heartbeat)(...);
    
    // Event bus
    esp_err_t (*post_event)(...);
    esp_err_t (*subscribe_event)(...);
    esp_err_t (*register_event_type)(...);
} app_context_t;
```

### Memory Utilities

```c
// App allocations (PSRAM)
void* APP_MALLOC(size_t size);
void* APP_CALLOC(size_t n, size_t size);

// System allocations (SRAM)
void* SYSTEM_MALLOC(size_t size);

// Memory info
memory_info_t info;
memory_get_info(&info);
memory_log_usage("tag");
```

## Examples

- **hello_app**: Basic app with event handling and memory management
- **goodbye_app**: App with countdown and network event subscription

## Partition Table

```
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000
phy_init, data, phy,     0xe000,  0x1000
factory,  app,  factory, 0x10000, 0x400000   # 4MB firmware
storage,  data, fat,     ,        0x100000   # 1MB for apps
```

Apps can be stored in the `storage` partition and loaded at runtime.

## Troubleshooting

### Build Issues

```bash
# Clean build
idf.py fullclean
idf.py build

# Check configuration
idf.py menuconfig
```

### Memory Issues

```c
// Check available memory
memory_log_usage("debug");

// Verify PSRAM
if (!memory_psram_available()) {
    ESP_LOGE(TAG, "PSRAM not available!");
}
```

### App Loading Issues

- Check app manifest is registered in main.c
- Verify CMakeLists.txt includes app source
- Check logs for initialization errors

## Contributing

1. Create your app in `components/apps/`
2. Follow memory management guidelines (PSRAM for apps)
3. Add documentation in `docs/`
4. Test with `idf.py build flash monitor`

## License

[Add your license here]

## Credits

Inspired by Flipper Zero's app architecture and ESP-IDF framework.
