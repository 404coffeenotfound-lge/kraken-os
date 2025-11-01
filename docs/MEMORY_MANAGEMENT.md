# ESP32-S3 Memory Management Strategy

## Overview

The Kraken system implements a clear separation between system service memory (SRAM) and application memory (PSRAM) to optimize performance and prevent resource exhaustion.

## Memory Architecture

### SRAM (Internal, Fast) - ~520KB
- **Purpose**: System services, WiFi/LWIP, RTOS, ISRs, critical data
- **Speed**: Fast access, no cache penalties
- **Allocation**: Automatic for system services using standard malloc/calloc

### PSRAM (External, Large) - Up to 32MB
- **Purpose**: Application data, large buffers, non-time-critical allocations
- **Speed**: Slower due to SPI/QSPI access, but much larger capacity
- **Allocation**: Explicit via heap_caps_malloc or APP_MALLOC macros

## Configuration (sdkconfig.defaults)

```
CONFIG_SPIRAM_USE_CAPS_ALLOC=y              # Explicit allocation model
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384   # Reserve 16KB SRAM minimum
CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y  # Allow static vars in PSRAM
```

This configuration ensures:
- System services default to SRAM (fast, deterministic)
- Apps must explicitly request PSRAM (prevents accidental SRAM exhaustion)
- WiFi/LWIP stay in SRAM for performance
- Minimum 16KB SRAM always reserved

## API Usage

### For Applications

```c
#include "system_service/memory_utils.h"

// Dynamic allocation in PSRAM
uint8_t *buffer = APP_MALLOC(50000);  // 50KB in PSRAM
free(buffer);

// Static allocation in PSRAM
APP_DATA_ATTR static uint8_t large_buffer[100000];

// Fallback strategy (prefer PSRAM, use SRAM if needed)
void *data = memory_alloc_prefer_psram(size);

// Strict PSRAM only (fails if PSRAM unavailable)
void *psram_data = memory_alloc_psram_only(size);
```

### For System Services

```c
// System services use standard allocation (goes to SRAM)
void *service_data = malloc(1024);

// Or explicitly request SRAM
void *critical_data = SYSTEM_MALLOC(2048);

// DMA-capable buffers (must be in SRAM)
void *dma_buffer = DMA_MALLOC(512);
```

### Memory Monitoring

```c
#include "system_service/memory_utils.h"

// Log detailed memory usage
memory_log_usage("my_app");

// Get memory info programmatically
memory_info_t info;
memory_get_info(&info);
ESP_LOGI(TAG, "Free SRAM: %zu bytes", info.free_sram);
ESP_LOGI(TAG, "Free PSRAM: %zu bytes", info.free_psram);

// Quick checks
size_t free_sram = memory_get_free_sram();
size_t free_psram = memory_get_free_psram();
bool has_psram = memory_psram_available();
```

## Best Practices

### DO:
✓ Use APP_MALLOC for app buffers > 4KB
✓ Use APP_DATA_ATTR for large static app arrays
✓ Keep WiFi, BLE, RTOS in SRAM (automatic with this config)
✓ Monitor memory usage during development
✓ Free PSRAM allocations when done

### DON'T:
✗ Don't use regular malloc in apps (wastes SRAM)
✗ Don't put ISR data in PSRAM (too slow)
✗ Don't put DMA buffers in PSRAM (not DMA-capable)
✗ Don't forget to check allocation failures

## Examples

### Example 1: Audio App with Large Buffers
```c
// hello_app.c
#include "system_service/memory_utils.h"

// Static audio buffer in PSRAM
APP_DATA_ATTR static int16_t audio_samples[48000];

esp_err_t hello_app_entry(app_context_t *ctx) {
    // Dynamic decode buffer in PSRAM
    uint8_t *decode_buffer = APP_MALLOC(100000);
    if (!decode_buffer) {
        ESP_LOGE(TAG, "Failed to allocate decode buffer");
        return ESP_ERR_NO_MEM;
    }
    
    // Use buffers...
    
    free(decode_buffer);
    return ESP_OK;
}
```

### Example 2: System Service with Critical Data
```c
// wifi_service.c
#include "system_service/memory_utils.h"

typedef struct {
    uint8_t control_data[256];  // In SRAM (automatic)
} wifi_ctx_t;

esp_err_t wifi_service_init(void) {
    // Allocate in SRAM for fast access
    wifi_ctx_t *ctx = SYSTEM_MALLOC(sizeof(wifi_ctx_t));
    if (!ctx) {
        return ESP_ERR_NO_MEM;
    }
    
    // Log to verify memory strategy
    memory_log_usage("wifi_service");
    
    return ESP_OK;
}
```

## Memory Layout Summary

```
┌─────────────────────────────────────┐
│           ESP32-S3 Memory           │
├─────────────────────────────────────┤
│  SRAM (~520KB) - Internal, Fast     │
│  ├─ RTOS Kernel                     │
│  ├─ WiFi/LWIP Stacks               │
│  ├─ System Services                 │
│  ├─ ISR Handlers                    │
│  └─ Critical Buffers                │
├─────────────────────────────────────┤
│  PSRAM (up to 32MB) - External      │
│  ├─ App Static Data                 │
│  ├─ App Dynamic Buffers             │
│  ├─ GUI Framebuffers                │
│  ├─ Audio/Image Data                │
│  └─ Large Datasets                  │
└─────────────────────────────────────┘
```

## Troubleshooting

### Out of Memory Errors
1. Check if using PSRAM: `memory_psram_available()`
2. Log memory usage: `memory_log_usage(TAG)`
3. Verify allocation location with heap_caps_get_info()

### Performance Issues
- Move time-critical data to SRAM
- Use SRAM for <4KB allocations
- Keep ISR data in SRAM

### PSRAM Not Available
- Check hardware (PSRAM chip installed?)
- Verify menuconfig: `idf.py menuconfig` → Component config → ESP PSRAM
- Check boot logs for PSRAM initialization

## References

- ESP-IDF Memory Types: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/memory-types.html
- External RAM Support: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/external-ram.html
