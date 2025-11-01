# Apps vs Services - Unified Architecture

## Overview

In Kraken, **both apps and services are first-class citizens** that integrate fully with `system_service`.

## Key Differences

### SERVICE
- **Purpose**: Background system component
- **Lifecycle**: Always running (started at boot)
- **Examples**: audio_service, bluetooth_service, display_service, network_service
- **Control**: Managed by system, rarely stopped

### APP
- **Purpose**: User-launched component
- **Lifecycle**: On-demand (can be started/stopped by user)
- **Examples**: hello app, goodbye app, calculator app, game app
- **Control**: Managed by app_manager + user

## What They Share (100% Compatible)

Both apps and services have **identical capabilities**:

```c
✅ Register with system_service
✅ Get service_id
✅ Post events to event_bus
✅ Subscribe to events
✅ Register custom event types
✅ Update state
✅ Send heartbeats
✅ Appear in system statistics
✅ Full interoperability
```

## Architecture Diagram

```
┌─────────────────────────────────────────────────────┐
│              SYSTEM_SERVICE (Core)                  │
│  - Service registry                                 │
│  - Event bus                                        │
│  - Statistics                                       │
└─────────────────────────────────────────────────────┘
           │                           │
    ┌──────┴────────┐         ┌────────┴─────────┐
    │   SERVICES    │         │   APP_MANAGER    │
    │  (Always on)  │         │  (On-demand)     │
    └───────────────┘         └──────────────────┘
           │                           │
    ┌──────┴────────┐         ┌────────┴─────────┐
    │               │         │                  │
┌───▼───┐  ┌───▼────┐    ┌───▼───┐    ┌────▼────┐
│ Audio │  │ BT     │    │ Hello │    │ Goodbye │
│       │  │        │    │       │    │         │
└───────┘  └────────┘    └───────┘    └─────────┘

All components register with system_service!
All components have full event_bus access!
```

## Registration Flow

### Service Registration (e.g., audio_service)

```c
// In audio_service_init()
system_service_register("audio_service", NULL, &service_id);

// Service is now:
// ✓ In system registry
// ✓ Has service_id
// ✓ Can use event_bus
// ✓ Visible in stats
```

### App Registration (e.g., hello app)

```c
// In app_manager_register_app()
// 1. Register with app_manager (for lifecycle)
app_manager_register_app(&hello_manifest, &info);

// 2. Inside app_manager, also registers with system_service
system_service_register(manifest->name, entry, &service_id);

// App is now:
// ✓ In app_manager registry (lifecycle)
// ✓ In system_service registry (integration)
// ✓ Has service_id
// ✓ Can use event_bus
// ✓ Visible in stats
```

## App Context - Full API Access

Apps get a context with **full system service API**:

```c
typedef struct app_context {
    app_info_t *app_info;
    system_service_id_t service_id;
    
    // Service management - FULL ACCESS
    esp_err_t (*register_service)(...);
    esp_err_t (*unregister_service)(...);
    esp_err_t (*set_state)(...);
    esp_err_t (*heartbeat)(...);
    
    // Event bus - FULL ACCESS
    esp_err_t (*post_event)(...);
    esp_err_t (*subscribe_event)(...);
    esp_err_t (*unsubscribe_event)(...);
    esp_err_t (*register_event_type)(...);
} app_context_t;
```

## Example: App Interacting with Services

```c
esp_err_t my_app_entry(app_context_t *ctx)
{
    // 1. Register custom event type
    system_event_type_t my_event;
    ctx->register_event_type("app.myapp.event", &my_event);
    
    // 2. Subscribe to service events (e.g., audio started)
    system_event_type_t audio_started;
    ctx->register_event_type("audio.started", &audio_started);
    ctx->subscribe_event(ctx->service_id, audio_started, 
                        my_handler, NULL);
    
    // 3. Post events to event bus
    ctx->post_event(ctx->service_id, my_event,
                   data, size, SYSTEM_EVENT_PRIORITY_NORMAL);
    
    // 4. Update state
    ctx->set_state(ctx->service_id, SYSTEM_SERVICE_STATE_RUNNING);
    
    // 5. Heartbeat
    ctx->heartbeat(ctx->service_id);
    
    return ESP_OK;
}
```

## Example: Service Interacting with Apps

```c
// In network_service.c

// Subscribe to app events
system_event_type_t hello_event;
system_event_register_type("app.hello.custom", &hello_event);
system_event_subscribe(network_service_id, hello_event,
                      network_app_event_handler, NULL);

// Handler receives events from apps
void network_app_event_handler(const system_event_t *event, void *ctx)
{
    ESP_LOGI("network", "Received event from app: type=%d", event->type);
    // Network service can react to app events!
}
```

## Communication Patterns

### 1. App → Service
```c
// App posts event
ctx->post_event(ctx->service_id, network_request_event, 
               &request_data, sizeof(request_data),
               SYSTEM_EVENT_PRIORITY_HIGH);

// Network service subscribed to this event, receives it
```

### 2. Service → App
```c
// Service posts event
system_event_post(audio_service_id, audio_volume_changed,
                 &volume_data, sizeof(volume_data),
                 SYSTEM_EVENT_PRIORITY_NORMAL);

// App subscribed to this event, receives it
```

### 3. App → App
```c
// Hello app posts event
ctx->post_event(ctx->service_id, hello_event,
               data, size, SYSTEM_EVENT_PRIORITY_NORMAL);

// Goodbye app subscribed, receives it
```

### 4. Service → Service
```c
// Audio service posts event
system_event_post(audio_id, audio_started, ...);

// Bluetooth service subscribed, receives it
```

## System Statistics

All components appear together:

```
System Statistics:
  Registered services: 6
  
  Services:
  [0] audio_service       State: RUNNING
  [1] bluetooth_service   State: RUNNING
  [2] display_service     State: RUNNING
  [3] network_service     State: RUNNING
  [4] hello               State: RUNNING  (app!)
  [5] goodbye             State: LOADED   (app!)
```

## Practical Use Cases

### Use Case 1: Calculator App needs Network

```c
esp_err_t calculator_app_entry(app_context_t *ctx)
{
    // Subscribe to network events
    system_event_type_t net_connected;
    ctx->register_event_type("network.connected", &net_connected);
    ctx->subscribe_event(ctx->service_id, net_connected,
                        on_network_ready, NULL);
    
    // When network connects, calculator can sync data
}
```

### Use Case 2: Game App controls Audio/Display

```c
esp_err_t game_app_entry(app_context_t *ctx)
{
    // Subscribe to audio and display events
    system_event_type_t audio_vol_changed;
    ctx->register_event_type("audio.volume_changed", &audio_vol_changed);
    ctx->subscribe_event(ctx->service_id, audio_vol_changed,
                        adjust_game_sound, NULL);
    
    // Post custom game events
    system_event_type_t game_score;
    ctx->register_event_type("game.score_updated", &game_score);
    ctx->post_event(ctx->service_id, game_score, &score, 
                   sizeof(score), SYSTEM_EVENT_PRIORITY_NORMAL);
}
```

### Use Case 3: Service reacts to App

```c
// In display_service.c

// Subscribe to all app events
system_event_type_t hello_event;
system_event_register_type("app.hello.custom", &hello_event);
system_event_subscribe(display_service_id, hello_event,
                      show_notification, NULL);

// When hello app posts event, display shows notification
void show_notification(const system_event_t *event, void *ctx)
{
    // Display service shows "Hello app is running!"
}
```

## Summary

### Before (Isolated)
```
Services → system_service ✓
Apps     → app_manager only ✗

Apps couldn't interact with services!
```

### After (Integrated) ✅
```
Services → system_service ✓
Apps     → app_manager + system_service ✓

Apps are first-class citizens!
Full bidirectional communication!
```

## Key Takeaways

1. **Apps = Services with lifecycle management**
   - Services: Always running
   - Apps: On-demand, managed by app_manager

2. **Both have identical system access**
   - Full event_bus access
   - Full service registration
   - Same visibility

3. **Perfect interoperability**
   - Apps can subscribe to service events
   - Services can subscribe to app events
   - Apps can communicate with each other
   - Services can communicate with each other

4. **Unified ecosystem**
   - Everything visible in system stats
   - Everything manageable
   - Everything monitored

This is the **Flipper Zero philosophy** - apps and services coexist as equals in the system!
