# System Service Architecture Design

## Overview
A secure, modular system service component for ESP-IDF v5.5.1 that provides centralized service management, event bus functionality, and system protection.

## Component Structure

```
components/system/
├── CMakeLists.txt              # ESP-IDF component build configuration
├── Kconfig                      # Configuration menu options
├── README.md                    # User documentation
│
├── include/system_service/      # PUBLIC HEADERS (exposed to users)
│   ├── system_types.h          # Core types and enums
│   ├── system_service.h        # Secure APIs (require key)
│   ├── service_manager.h       # Service registration APIs
│   └── event_bus.h             # Event publish/subscribe APIs
│
├── private/                     # PRIVATE HEADERS (internal only)
│   ├── system_internal.h       # Internal data structures
│   └── security.h              # Security key functions
│
├── src/                         # IMPLEMENTATION
│   ├── system_service.c        # Core system service
│   ├── service_manager.c       # Service management
│   ├── event_bus.c             # Event bus implementation
│   └── security.c              # Security functions
│
└── examples/                    # USAGE EXAMPLES
    └── basic_example.c         # Demonstration code
```

## Module Separation

### 1. **system_types.h** - Common Types
- Data types used across all modules
- Enums for states and priorities
- Callback function signatures
- No dependencies on other modules

### 2. **system_service.h/.c** - Core Service (SECURED)
**Requires secure key for all operations**
- `system_service_init()` - Initialize and generate key
- `system_service_start()` - Start event processing
- `system_service_stop()` - Stop event processing
- `system_service_deinit()` - Cleanup
- `system_service_get_stats()` - Get statistics

**Purpose**: Only main() can control system lifecycle

### 3. **service_manager.h/.c** - Service Registry (PUBLIC)
**No key required - any component can use**
- `system_service_register()` - Register service
- `system_service_unregister()` - Unregister service
- `system_service_set_state()` - Update state
- `system_service_get_state()` - Query state
- `system_service_heartbeat()` - Update heartbeat
- `system_service_get_info()` - Get service info
- `system_service_list_all()` - List all services

**Purpose**: Services register themselves and report health

### 4. **event_bus.h/.c** - Event System (PUBLIC)
**No key required - any component can use**
- `system_event_register_type()` - Register event type
- `system_event_subscribe()` - Subscribe to events
- `system_event_unsubscribe()` - Unsubscribe
- `system_event_post()` - Post event
- `system_event_post_async()` - Post async event
- `system_event_get_type_name()` - Get event name

**Purpose**: Inter-service communication

### 5. **security.h/.c** - Security (PRIVATE)
**Internal use only**
- `security_generate_key()` - Generate secure key
- `security_validate_key()` - Validate key
- `security_invalidate_key()` - Invalidate key

**Purpose**: Secure key generation and validation

### 6. **system_internal.h** - Internal Structures (PRIVATE)
**Internal use only**
- `system_context_t` - Global system state
- Service/event/subscription entries
- Internal helper functions

**Purpose**: Shared internal data structures

## Security Model

```
┌─────────────────────────────────────────────────────────┐
│  main.c (app_main)                                      │
│  ┌──────────────────────────────────────────────────┐   │
│  │  1. system_service_init(&key)  [Generates key]  │   │
│  │  2. system_service_start(key)  [Starts system]  │   │
│  │  3. Pass key only to trusted code               │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                        │
                        │ Secure Key Required
                        ▼
┌─────────────────────────────────────────────────────────┐
│  System Service (PROTECTED)                             │
│  - Init/Deinit/Start/Stop/Stats require key            │
│  - Key validated on every call                          │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│  Public APIs (NO KEY REQUIRED)                          │
│  ┌──────────────────┐    ┌──────────────────────┐      │
│  │ Service Manager  │    │    Event Bus         │      │
│  │ - Register       │    │ - Register events    │      │
│  │ - Unregister     │    │ - Subscribe          │      │
│  │ - Heartbeat      │    │ - Post events        │      │
│  │ - Get info       │    │ - Unsubscribe        │      │
│  └──────────────────┘    └──────────────────────┘      │
└─────────────────────────────────────────────────────────┘
```

## Data Flow

### Service Registration Flow
```
Service Component
    │
    ├─→ system_service_register("my_service", ctx, &id)
    │   └─→ Validates system initialized
    │       └─→ Finds free slot
    │           └─→ Stores service info
    │               └─→ Returns service_id
    │
    └─→ system_service_set_state(id, RUNNING)
        └─→ Updates service state
```

### Event Flow
```
Service A                    Event Bus                   Service B
    │                            │                            │
    ├─→ register_type("data")    │                            │
    │       └─→ Returns type_id  │                            │
    │                            │    ←─subscribe(type_id)────┤
    │                            │         Registers handler  │
    │                            │                            │
    ├─→ post_event(type_id, data)│                            │
    │       └─→ Queue event      │                            │
    │           (copies data)    │                            │
    │                            │                            │
    │                    Event Task processes queue          │
    │                            │                            │
    │                            ├─→ Calls handler(event)────→│
    │                            │                            │
    │                            └─→ Frees event data        │
```

## Thread Safety

### Mutex Protection
- All shared data structures protected by mutex
- Lock/unlock helpers in system_internal.h
- 1000ms timeout on lock acquisition

### Event Processing
- Dedicated FreeRTOS task for event processing
- Events queued with data copied (no shared pointers)
- Handlers called sequentially (not concurrent)

## Buffer Protection

### Size Limits
```c
#define SYSTEM_SERVICE_MAX_SERVICES     16   // Max services
#define SYSTEM_SERVICE_MAX_EVENT_TYPES  64   // Max event types
#define SYSTEM_SERVICE_MAX_SUBSCRIBERS  32   // Max subscriptions
#define SYSTEM_MAX_DATA_SIZE            512  // Max event data
#define SYSTEM_EVENT_QUEUE_SIZE         32   // Event queue depth
```

### Validation
- All array bounds checked
- Data size validated before malloc
- Queue full handling with timeout
- String operations use strncpy with null termination

## Usage Patterns

### Pattern 1: System Initialization (main only)
```c
void app_main(void) {
    system_secure_key_t key;
    system_service_init(&key);      // Generate key
    system_service_start(key);      // Start system
    
    // ... application code ...
    
    system_service_stop(key);       // Cleanup
    system_service_deinit(key);
}
```

### Pattern 2: Service Registration (any component)
```c
void my_component_init(void) {
    system_service_id_t id;
    system_service_register("my_service", ctx, &id);
    system_service_set_state(id, SYSTEM_SERVICE_STATE_RUNNING);
    
    // Periodic heartbeat
    system_service_heartbeat(id);
}
```

### Pattern 3: Event Communication (any component)
```c
// Publisher
system_event_type_t evt;
system_event_register_type("sensor_data", &evt);
system_event_post(service_id, evt, &data, sizeof(data), PRIORITY_NORMAL);

// Subscriber
void handler(const system_event_t *event, void *ctx) {
    // Process event
}
system_event_subscribe(service_id, evt, handler, ctx);
```

## Extensibility

### Adding New Features
1. **Add to appropriate module** - Don't mix concerns
2. **Update header** - Public API in include/, private in private/
3. **Implement in .c** - Use existing patterns
4. **Update Kconfig** - If configurable
5. **Document in README** - Usage examples

### Recommended Extensions
- Service watchdog timer (detect unresponsive services)
- Event priority queue (process critical events first)
- Service dependency tracking
- Event logging/debugging mode
- Service lifecycle callbacks
- Resource usage tracking

## Configuration

### Compile-time (system_types.h)
```c
#define SYSTEM_SERVICE_MAX_SERVICES     16
#define SYSTEM_SERVICE_MAX_EVENT_TYPES  64
// etc.
```

### Runtime (Kconfig via menuconfig)
- Maximum services/events/subscribers
- Queue sizes
- Task priority and stack size
- Timeouts

## Testing Strategy

1. **Unit Tests** - Test each module independently
2. **Integration Tests** - Test module interactions
3. **Security Tests** - Verify key validation works
4. **Stress Tests** - Max services, max events, queue full
5. **Concurrency Tests** - Multiple tasks using APIs

## Performance Considerations

- Event data is **copied** (safe but uses heap)
- Mutex locks may cause delays under contention
- Event handlers block event task
- Consider event priority for time-critical events

## Memory Usage

**Static (always allocated):**
- Service entries: 16 × ~60 bytes = ~960 bytes
- Event types: 64 × ~36 bytes = ~2.3 KB
- Subscriptions: 32 × ~16 bytes = ~512 bytes
- Total static: ~4 KB

**Dynamic (per event):**
- Queue entry: sizeof(system_event_t) = ~24 bytes
- Event data: 0 to SYSTEM_MAX_DATA_SIZE (512 bytes)
- Queue capacity: 32 events × ~536 bytes = ~17 KB max

**Task stack:** 4096 bytes (configurable)

**Total worst case:** ~25 KB

## Design Principles Applied

✅ **Separation of Concerns** - Each file has single responsibility
✅ **Encapsulation** - Private headers for internals
✅ **Security** - Key-based access control
✅ **Thread Safety** - Mutex-protected shared state
✅ **Resource Protection** - Size limits and validation
✅ **Modularity** - Independent compilation units
✅ **Extensibility** - Clear patterns for additions
✅ **Documentation** - Comprehensive README and examples
