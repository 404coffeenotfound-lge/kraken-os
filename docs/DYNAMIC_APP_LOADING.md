# Dynamic App Loading for Kraken OS

This document describes how to build, load, and run dynamic apps in Kraken OS, similar to Flipper Zero's app loading system.

## Overview

Kraken OS supports two types of apps:

1. **Built-in apps** - Compiled directly into the firmware (like `hello` and `goodbye` currently)
2. **Dynamic apps** - Loaded at runtime from flash partitions or SD card

Dynamic apps are compiled as Position-Independent Code (PIC) and can be:
- Loaded from flash partitions
- Loaded from SPIFFS/SD card
- Downloaded from network (future)

## Architecture

### System API Table

Dynamic apps access system services through a stable API table defined in `app_loader.h`:

```c
typedef struct {
    uint32_t version;
    
    // Service management
    esp_err_t (*register_service)(...);
    esp_err_t (*unregister_service)(...);
    esp_err_t (*set_state)(...);
    esp_err_t (*heartbeat)(...);
    
    // Event bus
    esp_err_t (*post_event)(...);
    esp_err_t (*subscribe_event)(...);
    
    // Memory, logging, FreeRTOS...
} system_api_table_t;
```

This table remains stable across firmware versions, allowing apps to work without recompilation.

### App Loader

The app loader (`app_loader.c`) handles:
- **ELF parsing** - Reads ELF32 format binaries
- **Memory allocation** - Allocates code (IRAM), data (PSRAM), and BSS segments
- **Relocation** - Applies Xtensa relocations for PIC code
- **Symbol resolution** - Links app to system API table
- **Execution** - Launches app as FreeRTOS task

## Building Dynamic Apps

### Step 1: Write Your App

Create your app in `components/apps/<app_name>/`:

```c
// my_app.c
#include "system_service/app_manager.h"
#include "esp_log.h"

static const char *TAG = "my_app";

esp_err_t my_app_entry(app_context_t *ctx)
{
    ESP_LOGI(TAG, "Hello from dynamic app!");
    
    // Use system APIs through context
    ctx->set_state(ctx->service_id, SYSTEM_SERVICE_STATE_RUNNING);
    ctx->heartbeat(ctx->service_id);
    
    // Subscribe to events
    system_event_type_t my_event;
    ctx->register_event_type("my_app.event", &my_event);
    ctx->post_event(ctx->service_id, my_event, NULL, 0, PRIORITY_NORMAL);
    
    return ESP_OK;
}
```

### Step 2: Build as PIC

Use the provided build script:

```bash
./build_pic_app.sh my_app
```

This will:
1. Compile your app with `-fPIC -fpic -mlongcalls`
2. Link as shared object
3. Generate ELF binary in `build/app_binaries/my_app.bin`

### Step 3: Flash to Partition

#### Option A: Add to partition table

Edit `partitions.csv`:

```csv
# Name,     Type, SubType, Offset,  Size,    Flags
nvs,        data, nvs,     0x9000,  0x6000,
phy_init,   data, phy,     0xf000,  0x1000,
factory,    app,  factory, 0x10000, 1M,
app_store,  data, fat,     ,        2M,      # Apps stored here
storage,    data, spiffs,  ,        1M,
```

#### Option B: Flash directly

```bash
# Find partition offset
python $IDF_PATH/components/partition_table/parttool.py \
    --port /dev/ttyUSB0 \
    read_partition --partition-name=app_store

# Flash app
esptool.py --port /dev/ttyUSB0 write_flash 0x<offset> \
    build/app_binaries/my_app.bin
```

### Step 4: Load at Runtime

```c
// In main/kraken.c or dynamically

// Load from partition
app_info_t *app_info = NULL;
esp_err_t ret = app_manager_load_dynamic_from_partition("app_store", 0, &app_info);

if (ret == ESP_OK) {
    // Start the app
    app_manager_start_app(app_info->manifest.name);
}
```

## Memory Layout

Dynamic apps use the following memory strategy:

```
┌─────────────────────────────────────┐
│  IRAM (Internal, Fast)              │
│  ├─ App code segment (executable)   │  ← Loaded here for performance
│  └─ Critical code paths             │
├─────────────────────────────────────┤
│  PSRAM (External, Large)            │
│  ├─ App data segment (.data)        │  ← Initialized data
│  ├─ App BSS segment (.bss)          │  ← Uninitialized data
│  └─ App heap allocations            │  ← malloc() via API table
└─────────────────────────────────────┘
```

## Building Apps for Different Targets

### For ESP32-S3 (Current)

```bash
./build_pic_app.sh my_app
```

Uses `xtensa-esp32s3-elf-gcc` with Xtensa-specific flags.

### For ESP32

Change `CC` in script to `xtensa-esp32-elf-gcc`.

### For ESP32-C3 (RISC-V)

Change `CC` to `riscv32-esp-elf-gcc` and adjust flags:
- Remove `-mlongcalls` (Xtensa-specific)
- Add RISC-V PIC flags

## Limitations

1. **No standard library** - Apps must use system APIs only
2. **No global constructors** - C++ constructors won't run automatically
3. **Size limits** - Apps limited by available IRAM/PSRAM
4. **API stability** - Apps compiled for API v1 won't work with breaking changes

## Advanced: Custom App Format

For more control, you can create a custom app format:

```c
typedef struct {
    uint32_t magic;             // "KRAK"
    char name[32];
    char version[16];
    uint32_t code_size;
    uint32_t data_size;
    uint32_t entry_offset;
    uint32_t crc32;
    // ... app code and data follows
} kraken_app_header_t;
```

Then modify `app_loader.c` to support both ELF and custom format.

## Examples

See `components/apps/hello` and `components/apps/goodbye` for examples.

To convert an existing built-in app to dynamic:

1. Keep the source code the same
2. Build with `build_pic_app.sh`
3. Flash to partition
4. Load with `app_manager_load_dynamic_from_partition()`

## Troubleshooting

### "Invalid ELF magic"

Ensure app is compiled as ELF32, not binary:
```bash
file build/apps/my_app/my_app.elf
# Should show: ELF 32-bit LSB shared object, Tensilica Xtensa
```

### "Failed to allocate code segment"

IRAM is full. Either:
- Reduce app code size
- Modify loader to use PSRAM for code (slower)

### "Relocation failed"

App was not built with `-fPIC`. Rebuild with correct flags.

### App crashes on execution

Common causes:
- Stack overflow - increase task stack size in `app_manager.c`
- Null pointer - ensure all API calls go through context
- Memory corruption - check buffer sizes

## Performance

**Built-in apps:**
- ✓ Fastest (no relocation overhead)
- ✓ Uses optimal memory layout
- ✗ Increases firmware size
- ✗ Requires reflash to update

**Dynamic apps:**
- ✓ Updatable without reflashing firmware
- ✓ Smaller firmware size
- ✗ Slight overhead from PIC indirection (~5%)
- ✗ More complex build process

## Future Enhancements

- [ ] App signing and verification
- [ ] Compressed app storage
- [ ] App marketplace/OTA updates
- [ ] App sandboxing (memory limits, resource quotas)
- [ ] Multi-app per partition
- [ ] App dependencies management
