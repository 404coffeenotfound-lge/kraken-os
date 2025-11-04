# Kraken Documentation

## Current Documentation Structure

### Root Level
- **README.md** - Main project overview, quick start, features
- **QUICK_REFERENCE.md** - Command cheat sheet and quick examples

### docs/ Folder
- **ARCHITECTURE.md** - System architecture, components, design
- **APPS_VS_SERVICES.md** - When to use apps vs services
- **MEMORY_MANAGEMENT.md** - SRAM/PSRAM allocation strategy

## Quick Navigation

**Getting Started:** README.md → QUICK_REFERENCE.md
**Understanding System:** ARCHITECTURE.md → APPS_VS_SERVICES.md
**Memory Strategy:** MEMORY_MANAGEMENT.md

## System Components

### Core Services (components/system)
- **system_service** - Secure system initialization
- **service_manager** - Service registration and health monitoring
- **event_bus** - Publish/subscribe event system
- **app_manager** - Application lifecycle management
- **common_events** - Pre-defined system event types
- **memory_utils** - Memory allocation and tracking
- **app_storage** - Application storage and persistence

### Built-in Apps (components/apps)
- **hello** - Example app demonstrating system API usage
- **goodbye** - Example app with event registration and posting

## Architecture Overview

```
┌─────────────────────────────────────────────────────┐
│                  Main Firmware                       │
│  ┌────────────────────────────────────────────────┐ │
│  │           System Components                    │ │
│  │  ├─ system_service (secure init)              │ │
│  │  ├─ service_manager (registration)            │ │
│  │  ├─ event_bus (pub/sub)                       │ │
│  │  ├─ app_manager (app lifecycle)               │ │
│  │  └─ common_events (standard events)           │ │
│  └────────────────────────────────────────────────┘ │
│  ┌────────────────────────────────────────────────┐ │
│  │           Built-in Apps                        │ │
│  │  ├─ hello (example app)                       │ │
│  │  └─ goodbye (example with events)             │ │
│  └────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────┘
```

Apps use system APIs via `app_context_t`:
- `ctx->set_state()` - Update service state
- `ctx->heartbeat()` - Report health
- `ctx->post_event()` - Publish events
- `ctx->register_event_type()` - Register custom events

