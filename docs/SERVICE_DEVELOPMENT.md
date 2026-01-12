# Service Development Guide

**Complete guide to creating production-ready services for Kraken-OS**

---

## Table of Contents

1. [Introduction](#introduction)
2. [Service Basics](#service-basics)
3. [Step-by-Step Tutorial](#step-by-step-tutorial)
4. [Production Features Integration](#production-features-integration)
5. [Best Practices](#best-practices)
6. [Complete Example](#complete-example)
7. [Testing and Debugging](#testing-and-debugging)

---

## Introduction

Services in Kraken-OS are **long-running system components** that provide hardware abstraction or system-level functionality. Unlike apps, services:

- ✅ Run continuously
- ✅ Provide APIs to other components
- ✅ Abstract hardware interfaces
- ✅ Are part of the firmware

**Examples**: Audio Service, Bluetooth Service, Display Service, Network Service

---

## Service Basics

### Service Lifecycle

```
UNREGISTERED → REGISTERED → RUNNING → STOPPING → STOPPED
     ↓              ↓           ↓          ↓         ↓
   init()      register()   start()    stop()   deinit()
```

### Required Components

Every service needs:
1. **Service ID** - Unique identifier
2. **Event Types** - Events the service publishes
3. **Initialization** - Setup and registration
4. **Start/Stop** - Lifecycle management
5. **Production Features** - Watchdog, quotas, heartbeat

---

## Step-by-Step Tutorial

### Step 1: Create Service Structure

**File**: `components/my_service/src/my_service.c`

```c
#include \"my_service.h\"
#include \"system_service/system_service.h\"
#include \"system_service/service_manager.h\"
#include \"system_service/event_bus.h\"
#include \"service_watchdog.h\"
#include \"resource_quota.h\"
#include \"esp_log.h\"

static const char *TAG = \"my_service\";

// Service state
static system_service_id_t my_service_id = 0;
static system_event_type_t my_events[4];
static bool initialized = false;
```

### Step 2: Define Event Types

**File**: `components/my_service/include/my_service.h`

```c
#ifndef MY_SERVICE_H
#define MY_SERVICE_H

#include \"esp_err.h\"
#include \"system_service/system_types.h\"

#ifdef __cplusplus
extern \"C\" {
#endif

/* Event indices */
typedef enum {
    MY_EVENT_REGISTERED = 0,
    MY_EVENT_STARTED,
    MY_EVENT_DATA_READY,
    MY_EVENT_ERROR
} my_event_index_t;

/* Event data structures */
typedef struct {
    uint32_t value;
    uint32_t timestamp;
} my_data_event_t;

/* Service API */
esp_err_t my_service_init(void);
esp_err_t my_service_deinit(void);
esp_err_t my_service_start(void);
esp_err_t my_service_stop(void);
system_service_id_t my_service_get_id(void);

#ifdef __cplusplus
}
#endif

#endif // MY_SERVICE_H
```

### Step 3: Implement Initialization

```c
esp_err_t my_service_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, \"Service already initialized\");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret;
    
    ESP_LOGI(TAG, \"Initializing my_service...\");
    
    // Step 3.1: Register with system service
    ret = system_service_register(\"my_service\", NULL, &my_service_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, \"Failed to register: %s\", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, \"✓ Registered with system (ID: %d)\", my_service_id);
    
    // Step 3.2: Register event types
    const char *event_names[] = {
        \"my_service.registered\",
        \"my_service.started\",
        \"my_service.data_ready\",
        \"my_service.error\"
    };
    
    for (int i = 0; i < 4; i++) {
        ret = system_event_register_type(event_names[i], &my_events[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, \"Failed to register event '%s'\", event_names[i]);
            return ret;
        }
    }
    ESP_LOGI(TAG, \"✓ Registered %d event types\", 4);
    
    // Step 3.3: Register with watchdog
    service_watchdog_config_t watchdog_config = {
        .timeout_ms = 30000,              // 30 second timeout
        .auto_restart = true,             // Auto-restart on failure
        .max_restart_attempts = 3,        // Max 3 restart attempts
        .is_critical = false              // Not a critical service
    };
    watchdog_register_service(my_service_id, &watchdog_config);
    ESP_LOGI(TAG, \"✓ Registered with watchdog (30s timeout)\");
    
    // Step 3.4: Set resource quotas
    service_quota_t quota = {
        .max_events_per_sec = 50,         // Max 50 events/sec
        .max_subscriptions = 8,           // Max 8 subscriptions
        .max_event_data_size = 256,       // Max 256 bytes per event
        .max_memory_bytes = 32 * 1024     // Max 32KB memory
    };
    quota_set(my_service_id, &quota);
    ESP_LOGI(TAG, \"✓ Resource quotas set (50 events/s, 32KB memory)\");
    
    // Step 3.5: Initialize hardware (if needed)
    // ... your hardware initialization code ...
    
    // Step 3.6: Set service state
    system_service_set_state(my_service_id, SYSTEM_SERVICE_STATE_REGISTERED);
    
    initialized = true;
    
    // Step 3.7: Post registration event
    system_event_post(my_service_id, 
                     my_events[MY_EVENT_REGISTERED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, \"✓ Service initialized successfully\");
    
    return ESP_OK;
}
```

### Step 4: Implement Start/Stop

```c
esp_err_t my_service_start(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, \"Starting my_service...\");
    
    // Update state
    system_service_set_state(my_service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    // Start hardware/tasks if needed
    // ... your start code ...
    
    // Send heartbeat to watchdog
    system_service_heartbeat(my_service_id);
    
    // Post started event
    system_event_post(my_service_id,
                     my_events[MY_EVENT_STARTED],
                     NULL, 0,
                     SYSTEM_EVENT_PRIORITY_NORMAL);
    
    ESP_LOGI(TAG, \"✓ Service started\");
    
    return ESP_OK;
}

esp_err_t my_service_stop(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, \"Stopping my_service...\");
    
    // Update state
    system_service_set_state(my_service_id, SYSTEM_SERVICE_STATE_STOPPING);
    
    // Stop hardware/tasks
    // ... your stop code ...
    
    ESP_LOGI(TAG, \"✓ Service stopped\");
    
    return ESP_OK;
}

esp_err_t my_service_deinit(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, \"Deinitializing my_service...\");
    
    // Unregister from system
    system_service_unregister(my_service_id);
    
    initialized = false;
    
    ESP_LOGI(TAG, \"✓ Service deinitialized\");
    
    return ESP_OK;
}
```

### Step 5: Implement Service Operations

```c
// Example: Process data and post event
esp_err_t my_service_process_data(uint32_t value)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Send heartbeat (keeps watchdog happy)
    system_service_heartbeat(my_service_id);
    
    // Prepare event data
    my_data_event_t event_data = {
        .value = value,
        .timestamp = esp_timer_get_time() / 1000
    };
    
    // Post event with HIGH priority
    esp_err_t ret = system_event_post(my_service_id,
                                      my_events[MY_EVENT_DATA_READY],
                                      &event_data,
                                      sizeof(event_data),
                                      SYSTEM_EVENT_PRIORITY_HIGH);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, \"Failed to post event: %s\", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, \"Data processed: %lu\", value);
    
    return ESP_OK;
}
```

### Step 6: Create CMakeLists.txt

**File**: `components/my_service/CMakeLists.txt`

```cmake
idf_component_register(
    SRCS \"src/my_service.c\"
    INCLUDE_DIRS \"include\"
    PRIV_INCLUDE_DIRS \"../system/private\"
    REQUIRES system
)
```

---

## Production Features Integration

### Watchdog Configuration

Choose timeout based on your service's characteristics:

```c
service_watchdog_config_t watchdog_config = {
    .timeout_ms = 30000,              // Adjust based on service
    .auto_restart = true,             // Usually true
    .max_restart_attempts = 3,        // 2-5 is typical
    .is_critical = false              // Only for essential services
};
```

**Guidelines:**
- **Fast services** (< 1s operations): 10-20s timeout
- **Normal services** (1-5s operations): 30s timeout
- **Slow services** (BT, Network): 45-60s timeout

### Resource Quotas

Set quotas based on expected usage:

```c
service_quota_t quota = {
    .max_events_per_sec = 50,         // Events you'll post
    .max_subscriptions = 8,           // Events you'll subscribe to
    .max_event_data_size = 256,       // Largest event data
    .max_memory_bytes = 32 * 1024     // Total memory budget
};
```

**Guidelines:**
- **Low-frequency services**: 10-20 events/s
- **Medium-frequency services**: 30-50 events/s
- **High-frequency services**: 80-100 events/s

### Heartbeat Strategy

Send heartbeats at key points:

```c
// In periodic tasks
void my_service_task(void *arg) {
    while (running) {
        // Do work
        process_data();
        
        // Send heartbeat every iteration
        system_service_heartbeat(my_service_id);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// In event handlers
void my_event_handler(const system_event_t *event, void *user_data) {
    // Process event
    handle_event(event);
    
    // Send heartbeat after processing
    system_service_heartbeat(my_service_id);
}

// In API calls
esp_err_t my_service_api_call(void) {
    // Do operation
    perform_operation();
    
    // Send heartbeat
    system_service_heartbeat(my_service_id);
    
    return ESP_OK;
}
```

**Rule of thumb**: Heartbeat at least every `timeout_ms / 2`

---

## Best Practices

### 1. Error Handling

```c
// ✅ GOOD: Specific error codes
esp_err_t ret = system_event_post(...);
if (ret == ESP_ERR_QUOTA_EVENTS_EXCEEDED) {
    ESP_LOGW(TAG, \"Event rate limit exceeded, throttling\");
    vTaskDelay(pdMS_TO_TICKS(100));
} else if (ret != ESP_OK) {
    ESP_LOGE(TAG, \"Failed to post event: %s\", esp_err_to_name(ret));
    return ret;
}

// ❌ BAD: Generic error handling
if (ret != ESP_OK) {
    return ret;  // No logging, no specific handling
}
```

### 2. Event Priority

```c
// ✅ GOOD: Appropriate priorities
system_event_post(..., SYSTEM_EVENT_PRIORITY_CRITICAL);  // System failure
system_event_post(..., SYSTEM_EVENT_PRIORITY_HIGH);      // User input
system_event_post(..., SYSTEM_EVENT_PRIORITY_NORMAL);    // Status update
system_event_post(..., SYSTEM_EVENT_PRIORITY_LOW);       // Background task

// ❌ BAD: Everything is CRITICAL
system_event_post(..., SYSTEM_EVENT_PRIORITY_CRITICAL);  // Status update
```

### 3. Resource Cleanup

```c
// ✅ GOOD: Proper cleanup
esp_err_t my_service_deinit(void) {
    // Unsubscribe from events
    for (int i = 0; i < num_subscriptions; i++) {
        system_event_unsubscribe(my_service_id, subscribed_events[i]);
    }
    
    // Stop tasks
    stop_all_tasks();
    
    // Free resources
    cleanup_resources();
    
    // Unregister from system
    system_service_unregister(my_service_id);
    
    return ESP_OK;
}

// ❌ BAD: Memory leaks
esp_err_t my_service_deinit(void) {
    system_service_unregister(my_service_id);
    return ESP_OK;  // Leaked subscriptions, tasks, resources
}
```

### 4. Thread Safety

```c
// ✅ GOOD: Protected shared state
static SemaphoreHandle_t state_mutex;

void update_state(int new_value) {
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    shared_state = new_value;
    xSemaphoreGive(state_mutex);
}

// ❌ BAD: Race conditions
void update_state(int new_value) {
    shared_state = new_value;  // Not thread-safe!
}
```

### 5. Logging

```c
// ✅ GOOD: Informative logging
ESP_LOGI(TAG, \"✓ Service initialized (ID: %d)\", service_id);
ESP_LOGW(TAG, \"Quota exceeded, throttling (rate: %d/s)\", current_rate);
ESP_LOGE(TAG, \"Hardware init failed: %s\", esp_err_to_name(ret));

// ❌ BAD: Useless logging
ESP_LOGI(TAG, \"OK\");
ESP_LOGE(TAG, \"Error\");
```

---

## Complete Example

See [example_service.c](example_service.c) for a complete, production-ready service implementation.

---

## Testing and Debugging

### Unit Testing

```c
void test_my_service(void) {
    // Initialize
    ESP_ERROR_CHECK(my_service_init());
    
    // Start
    ESP_ERROR_CHECK(my_service_start());
    
    // Test operations
    ESP_ERROR_CHECK(my_service_process_data(42));
    
    // Verify state
    system_service_state_t state;
    system_service_get_state(my_service_id, &state);
    assert(state == SYSTEM_SERVICE_STATE_RUNNING);
    
    // Stop
    ESP_ERROR_CHECK(my_service_stop());
    
    // Cleanup
    ESP_ERROR_CHECK(my_service_deinit());
}
```

### Debugging Tips

**1. Check Service Registration:**
```c
system_service_info_t info;
esp_err_t ret = system_service_get_info(my_service_id, &info);
if (ret == ESP_OK) {
    ESP_LOGI(TAG, \"Service: %s, State: %d\", info.name, info.state);
}
```

**2. Monitor Watchdog:**
```c
// Check if watchdog is triggering
watchdog_log_status(TAG);
```

**3. Check Quotas:**
```c
// See if you're hitting limits
quota_log_status(TAG);
```

**4. Monitor Events:**
```c
// Enable event bus logging
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
```

---

## Checklist

Before deploying your service:

- [ ] Service registers with system
- [ ] All event types registered
- [ ] Watchdog configured and registered
- [ ] Resource quotas set
- [ ] Heartbeats sent regularly
- [ ] Error handling implemented
- [ ] Resource cleanup in deinit
- [ ] Thread-safe shared state
- [ ] Appropriate event priorities
- [ ] Comprehensive logging
- [ ] Tested init/start/stop/deinit
- [ ] Tested under load
- [ ] Tested fault injection
- [ ] Documentation written

---

## Next Steps

- Review [ARCHITECTURE.md](ARCHITECTURE.md) for system overview
- See [example_service.c](example_service.c) for complete example
- Read [PRODUCTION_FEATURES.md](PRODUCTION_FEATURES.md) for advanced features
- Check [API_REFERENCE.md](API_REFERENCE.md) for detailed API docs

**Happy service development!** 🚀
