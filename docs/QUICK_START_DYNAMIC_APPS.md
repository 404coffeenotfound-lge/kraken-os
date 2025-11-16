# Quick Start Guide: Building and Loading Dynamic Apps

This guide walks you through building and loading your first dynamic app in Kraken OS.

## Prerequisites

- ESP-IDF v5.5.1 installed and sourced
- Kraken OS firmware flashed to device
- Partition table with `app_store` partition (already configured in `partitions.csv`)

## Step 1: Build the Main Firmware

```bash
cd /path/to/kraken-os
idf.py build
idf.py flash monitor
```

The firmware now includes the dynamic app loader!

## Step 2: Build a Dynamic App

Let's build the `hello` app as a dynamic app:

```bash
# Make sure ESP-IDF is sourced
source ~/esp/esp-idf/export.sh

# Build the hello app as PIC
./build_pic_app.sh hello
```

This creates:
- `build/apps/hello/hello.elf` - ELF executable
- `build/app_binaries/hello.bin` - Binary for flashing

## Step 3: Flash the Dynamic App

The app needs to be flashed to the `app_store` partition:

```bash
# Find the partition offset
python $IDF_PATH/components/partition_table/parttool.py \
    --port /dev/ttyUSB0 \
    get_partition_info --partition-name app_store

# Flash the app (assuming app_store starts at 0x410000)
esptool.py --port /dev/ttyUSB0 write_flash \
    0x410000 build/app_binaries/hello.bin
```

Or use the partition tool directly:

```bash
python $IDF_PATH/components/partition_table/parttool.py \
    --port /dev/ttyUSB0 \
    write_partition --partition-name=app_store \
    --input=build/app_binaries/hello.bin
```

## Step 4: Load and Run the App

In your `main/kraken.c`, add:

```c
#include "system_service/app_loader.h"

// In your main task or app_main:
ESP_LOGI(TAG, "Loading dynamic app from partition...");

app_info_t *app_info = NULL;
esp_err_t ret = app_manager_load_dynamic_from_partition("app_store", 0, &app_info);

if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Dynamic app loaded: %s", app_info->manifest.name);
    
    // Start the app
    app_manager_start_app(app_info->manifest.name);
} else {
    ESP_LOGE(TAG, "Failed to load dynamic app: %s", esp_err_to_name(ret));
}
```

## Step 5: Monitor the Output

```bash
idf.py monitor
```

You should see:

```
I (123) app_loader: Initializing app loader...
I (124) app_loader: App loader initialized (API version 1)
I (125) app_loader: Loading app from partition 'app_store' at offset 0
I (130) app_loader: ELF header valid:
I (131) app_loader:   Type:         3
I (132) app_loader:   Machine:      94 (Xtensa)
I (133) app_loader:   Entry:        0x00000000
I (134) app_loader:   Sections:     12
I (140) app_loader: ✓ Allocated code segment at 0x...
I (145) app_loader: ✓ Allocated data segment at 0x...
I (150) app_loader: ✓ App loaded successfully
I (155) app_manager: ✓ Loaded dynamic app 'dynamic_app_0' from partition
I (160) hello_app: ╔══════════════════════════════════════════╗
I (161) hello_app: ║         HELLO APP STARTED!               ║
I (162) hello_app: ╚══════════════════════════════════════════╝
```

## Troubleshooting

### Build Issues

**Error: "xtensa-esp32s3-elf-gcc: command not found"**

Solution: Source ESP-IDF:
```bash
source ~/esp/esp-idf/export.sh
```

**Error: "IDF_PATH not set"**

Solution: Set IDF_PATH:
```bash
export IDF_PATH=~/esp/esp-idf
```

### Loading Issues

**Error: "Invalid ELF magic"**

Solution: The binary wasn't built correctly. Rebuild:
```bash
rm -rf build/apps/hello
./build_pic_app.sh hello
```

**Error: "Partition 'app_store' not found"**

Solution: Flash the partition table:
```bash
idf.py partition-table-flash
```

**Error: "Failed to allocate code segment"**

Solution: Not enough IRAM. Options:
1. Reduce app code size
2. Modify `app_loader.c` to use PSRAM for code (slower)

### Runtime Issues

**App loads but crashes**

Common causes:
1. Stack overflow - increase stack in `app_manager.c:224`
2. Null pointer - ensure app uses `ctx->` for all system calls
3. Incorrect relocation - rebuild with `-fPIC`

Enable debug logging:
```c
esp_log_level_set("app_loader", ESP_LOG_DEBUG);
esp_log_level_set("app_manager", ESP_LOG_DEBUG);
```

## Advanced Usage

### Building Multiple Apps

```bash
./build_pic_app.sh hello
./build_pic_app.sh goodbye
```

### Loading Multiple Apps from Different Offsets

```c
// Load hello at offset 0
app_manager_load_dynamic_from_partition("app_store", 0, &hello_info);

// Load goodbye at offset 0x10000 (64KB)
app_manager_load_dynamic_from_partition("app_store", 0x10000, &goodbye_info);
```

### Creating a Custom App

1. Create directory:
```bash
mkdir -p components/apps/my_app
```

2. Create `my_app.c`:
```c
#include "system_service/app_manager.h"
#include "esp_log.h"

static const char *TAG = "my_app";

esp_err_t my_app_entry(app_context_t *ctx)
{
    ESP_LOGI(TAG, "My app is running!");
    
    // Use system APIs
    ctx->set_state(ctx->service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    return ESP_OK;
}
```

3. Build and flash:
```bash
./build_pic_app.sh my_app
python $IDF_PATH/components/partition_table/parttool.py \
    --port /dev/ttyUSB0 \
    write_partition --partition-name=app_store \
    --input=build/app_binaries/my_app.bin
```

## Next Steps

- Read `docs/DYNAMIC_APP_LOADING.md` for architecture details
- See `docs/example_dynamic_apps.c` for more examples
- Explore the system API in `components/system/include/system_service/app_loader.h`
- Add app signing and verification for security

## Performance Comparison

| Type | Pros | Cons |
|------|------|------|
| **Built-in** | Fast, optimal layout | Firmware reflash to update |
| **Dynamic** | Updatable, smaller firmware | ~5% PIC overhead |

Choose built-in for core apps, dynamic for user/optional apps!
