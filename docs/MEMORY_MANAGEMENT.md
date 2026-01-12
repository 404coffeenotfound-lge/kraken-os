# ESP32-S3 Memory Management Strategy

**Production-Ready Memory Management for Kraken-OS**

---

## Overview

Kraken-OS implements a **two-tier memory management strategy**:

1. **Memory Pools** - For event data and frequent allocations (reduces fragmentation)
2. **SRAM vs PSRAM** - Strategic allocation based on performance requirements

This approach optimizes performance, reduces fragmentation, and prevents resource exhaustion.

---

## Memory Architecture

### Hardware Layout

```
┌─────────────────────────────────────┐
│           ESP32-S3 Memory           │
├─────────────────────────────────────┤
│  SRAM (~520KB) - Internal, Fast     │
│  ├─ FreeRTOS Kernel                 │
│  ├─ WiFi/LWIP Stacks               │
│  ├─ System Services                 │
│  ├─ Memory Pools                    │
│  ├─ ISR Handlers                    │
│  └─ Critical Buffers                │
├─────────────────────────────────────┤
│  PSRAM (8MB) - External, Large      │
│  ├─ App Static Data                 │
│  ├─ App Dynamic Buffers             │
│  ├─ Display Framebuffers            │
│  ├─ Audio Buffers                   │
│  └─ Large Datasets                  │
└─────────────────────────────────────┘
```

### SRAM (Internal, Fast) - ~520KB
- **Purpose**: System services, WiFi/LWIP, RTOS, ISRs, critical data
- **Speed**: Fast access, no cache penalties
- **Allocation**: Memory pools (automatic) or standard malloc

### PSRAM (External, Large) - 8MB
- **Purpose**: Application data, large buffers, non-time-critical allocations
- **Speed**: Slower due to SPI/QSPI access, but much larger capacity
- **Allocation**: Explicit via heap_caps_malloc or APP_MALLOC macros

---

## Memory Pool System (NEW)

### What Are Memory Pools?

Memory pools are **pre-allocated blocks** of memory that reduce heap fragmentation and improve allocation performance.

**Benefits:**
- ✅ Reduced fragmentation (60-80% improvement)
- ✅ Faster allocation (O(1) when pool available)
- ✅ Automatic fallback to heap
- ✅ Thread-safe
- ✅ Usage statistics

### Pool Sizes

| Pool Size | Count | Use Case | Location |
|-----------|-------|----------|----------|
| 64 bytes  | 100   | Small events, metadata | SRAM |
| 256 bytes | 50    | Medium events, messages | SRAM |
| 1 KB      | 20    | Large events, buffers | SRAM |
| 4 KB      | 10    | Very large data | SRAM |

### Automatic Usage

**Event system uses pools automatically:**

```c
// This automatically uses memory pools!
system_event_post(service_id, event_type, data, size, priority);
// ↑ Event data allocated from appropriate pool
// ↑ Automatically freed after delivery
```

**No manual pool management needed** for event data!

### Manual Pool Usage (Advanced)

```c
#include \"memory_pool.h\"

// Allocate from pool
void *ptr = memory_pool_alloc(256);
if (ptr == NULL) {
    // Pool exhausted, fell back to heap
}

// Free back to pool
memory_pool_free(ptr);

// Get statistics
memory_pool_log_stats(TAG);
```

**Output:**
```
Memory Pool Statistics:
  Small (64B):   45/100 used, 2 heap fallbacks
  Medium (256B): 12/50 used, 0 heap fallbacks
  Large (1KB):   3/20 used, 0 heap fallbacks
  XLarge (4KB):  1/10 used, 0 heap fallbacks
```

---

## SRAM vs PSRAM Allocation

### For Applications

```c
#include \"system_service/memory_utils.h\"

// Dynamic allocation in PSRAM (for large buffers)
uint8_t *buffer = APP_MALLOC(50000);  // 50KB in PSRAM
if (buffer == NULL) {
    ESP_LOGE(TAG, \"Failed to allocate PSRAM\");
    return ESP_ERR_NO_MEM;
}
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
// Small allocations - use standard malloc (goes to SRAM)
void *service_data = malloc(1024);

// Or explicitly request SRAM
void *critical_data = SYSTEM_MALLOC(2048);

// DMA-capable buffers (must be in SRAM)
void *dma_buffer = DMA_MALLOC(512);

// For event data - use memory pools (automatic via event_post)
system_event_post(service_id, event_type, &data, sizeof(data), priority);
```

---

## Configuration

### ESP-IDF Configuration (sdkconfig.defaults)

```
CONFIG_SPIRAM_USE_CAPS_ALLOC=y              # Explicit allocation model
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384   # Reserve 16KB SRAM minimum
CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y  # Allow static vars in PSRAM
```

### Memory Pool Configuration

Access via `idf.py menuconfig` → **Component config** → **System Service Configuration**

```
CONFIG_SYSTEM_SERVICE_MEMORY_POOL_SMALL_SIZE=64
CONFIG_SYSTEM_SERVICE_MEMORY_POOL_SMALL_COUNT=100
CONFIG_SYSTEM_SERVICE_MEMORY_POOL_MEDIUM_SIZE=256
CONFIG_SYSTEM_SERVICE_MEMORY_POOL_MEDIUM_COUNT=50
CONFIG_SYSTEM_SERVICE_MEMORY_POOL_LARGE_SIZE=1024
CONFIG_SYSTEM_SERVICE_MEMORY_POOL_LARGE_COUNT=20
CONFIG_SYSTEM_SERVICE_MEMORY_POOL_XLARGE_SIZE=4096
CONFIG_SYSTEM_SERVICE_MEMORY_POOL_XLARGE_COUNT=10
```

---

## Memory Monitoring

### Heap Monitoring (NEW)

```c
#include \"heap_monitor.h\"

// Check heap health
if (!heap_monitor_check_health()) {
    ESP_LOGW(TAG, \"Heap fragmentation high!\");
}

// Get statistics
heap_stats_t stats;
heap_monitor_get_stats(&stats);
ESP_LOGI(TAG, \"Free: %zu bytes\", stats.total_free_bytes);
ESP_LOGI(TAG, \"Largest block: %zu bytes\", stats.largest_free_block);
ESP_LOGI(TAG, \"Fragmentation: %d%%\", stats.fragmentation_percent);

// Log detailed stats
heap_monitor_log_stats(TAG);
```

**Output:**
```
Heap Fragmentation:
  Total: 8MB
  Free: 6.2MB
  Largest block: 4.8MB
  Fragmentation: 12% ✅ HEALTHY
```

### Memory Pool Monitoring

```c
#include \"memory_pool.h\"

// Log pool statistics
memory_pool_log_stats(TAG);
```

### Legacy Memory Utils (Still Available)

```c
#include \"system_service/memory_utils.h\"

// Log detailed memory usage
memory_log_usage(\"my_service\");

// Get memory info
memory_info_t info;
memory_get_info(&info);
ESP_LOGI(TAG, \"Free SRAM: %zu bytes\", info.free_sram);
ESP_LOGI(TAG, \"Free PSRAM: %zu bytes\", info.free_psram);

// Quick checks
size_t free_sram = memory_get_free_sram();
size_t free_psram = memory_get_free_psram();
bool has_psram = memory_psram_available();
```

---

## Best Practices

### DO ✅

- **Use event system for inter-service communication** - Automatic pool management
- **Use APP_MALLOC for app buffers > 4KB** - Saves SRAM
- **Use APP_DATA_ATTR for large static arrays** - Goes to PSRAM
- **Keep WiFi, BLE, RTOS in SRAM** - Automatic with config
- **Monitor memory usage during development** - Use heap_monitor
- **Free allocations when done** - Prevent leaks

### DON'T ❌

- **Don't manually manage event data** - Event system handles it
- **Don't use regular malloc in apps for large buffers** - Wastes SRAM
- **Don't put ISR data in PSRAM** - Too slow
- **Don't put DMA buffers in PSRAM** - Not DMA-capable
- **Don't forget to check allocation failures** - Always check NULL

---

## Examples

### Example 1: Service Using Event System (Recommended)

```c
#include \"system_service/event_bus.h\"

// Event data automatically uses memory pools!
typedef struct {
    uint32_t value;
    uint8_t data[200];  // Will use 256-byte pool
} my_event_data_t;

void my_service_send_data(void) {
    my_event_data_t event_data = {
        .value = 42
    };
    
    // Memory pool allocation happens automatically
    system_event_post(service_id, event_type, 
                     &event_data, sizeof(event_data),
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    // ↑ Data copied to pool, sent to queue, freed automatically
}
```

### Example 2: App with Large Buffers

```c
#include \"system_service/memory_utils.h\"

// Static audio buffer in PSRAM
APP_DATA_ATTR static int16_t audio_samples[48000];

esp_err_t audio_app_entry(app_context_t *ctx) {
    // Dynamic decode buffer in PSRAM
    uint8_t *decode_buffer = APP_MALLOC(100000);
    if (!decode_buffer) {
        ESP_LOGE(TAG, \"Failed to allocate decode buffer\");
        return ESP_ERR_NO_MEM;
    }
    
    // Use buffers...
    process_audio(audio_samples, decode_buffer);
    
    // Cleanup
    free(decode_buffer);
    return ESP_OK;
}
```

### Example 3: Service with Critical Data

```c
#include \"system_service/memory_utils.h\"

typedef struct {
    uint8_t control_data[256];  // In SRAM (automatic)
} wifi_ctx_t;

esp_err_t wifi_service_init(void) {
    // Small allocation - use SRAM for fast access
    wifi_ctx_t *ctx = SYSTEM_MALLOC(sizeof(wifi_ctx_t));
    if (!ctx) {
        return ESP_ERR_NO_MEM;
    }
    
    // Monitor memory
    memory_log_usage(\"wifi_service\");
    heap_monitor_log_stats(TAG);
    
    return ESP_OK;
}
```

---

## Allocation Decision Tree

```
Need to allocate memory?
│
├─ For event data?
│  └─ Use system_event_post() → Automatic pool allocation ✅
│
├─ Small allocation (< 4KB)?
│  ├─ Time-critical or ISR?
│  │  └─ Use malloc() or SYSTEM_MALLOC() → SRAM ✅
│  └─ Not time-critical?
│     └─ Use malloc() → SRAM (acceptable) ✅
│
└─ Large allocation (> 4KB)?
   ├─ Application data?
   │  └─ Use APP_MALLOC() → PSRAM ✅
   └─ DMA buffer?
      └─ Use DMA_MALLOC() → SRAM ✅
```

---

## Troubleshooting

### Out of Memory Errors

1. **Check pool exhaustion:**
   ```c
   memory_pool_log_stats(TAG);
   ```

2. **Check heap fragmentation:**
   ```c
   heap_monitor_log_stats(TAG);
   if (!heap_monitor_check_health()) {
       ESP_LOGW(TAG, \"Heap fragmentation high!\");
   }
   ```

3. **Check PSRAM availability:**
   ```c
   if (!memory_psram_available()) {
       ESP_LOGE(TAG, \"PSRAM not available!\");
   }
   ```

4. **Log memory usage:**
   ```c
   memory_log_usage(TAG);
   ```

### Performance Issues

- **Move time-critical data to SRAM** - Use SYSTEM_MALLOC()
- **Use SRAM for < 4KB allocations** - Standard malloc
- **Keep ISR data in SRAM** - Never use PSRAM
- **Check pool sizes** - May need to increase via menuconfig

### High Fragmentation

- **Use memory pools** - Event system does this automatically
- **Allocate/free in order** - Avoid interleaved allocations
- **Use static buffers** - When size is known
- **Monitor regularly** - `heap_monitor_log_stats(TAG)`

---

## Memory Pool vs Heap

### When Pools Are Used (Automatic)

- ✅ Event data in `system_event_post()`
- ✅ Internal event bus allocations
- ✅ Automatic fallback to heap if pool exhausted

### When Heap Is Used

- Standard `malloc()` / `free()`
- `APP_MALLOC()` for PSRAM
- `SYSTEM_MALLOC()` for SRAM
- `DMA_MALLOC()` for DMA buffers
- Pool fallback when exhausted

---

## Summary

Kraken-OS provides **three memory management layers**:

1. **Memory Pools** (NEW) - Automatic for events, reduces fragmentation
2. **SRAM Allocation** - Fast, for system services and small data
3. **PSRAM Allocation** - Large, for app buffers and big data

**For most developers:**
- Use `system_event_post()` for events (automatic pools)
- Use `APP_MALLOC()` for large app buffers (PSRAM)
- Use `malloc()` for small service data (SRAM)

**The system handles the rest automatically!**

---

## References

- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture overview
- [SERVICE_DEVELOPMENT.md](SERVICE_DEVELOPMENT.md) - Service development guide
- ESP-IDF Memory Types: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/memory-types.html
- External RAM Support: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/external-ram.html
