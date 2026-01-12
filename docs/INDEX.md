# Kraken-OS Documentation

**Production-Ready ESP32-S3 Service-Oriented Operating System**

Welcome to Kraken-OS! This documentation will guide you through understanding, using, and extending this enterprise-grade embedded operating system.

---

## 🚀 Quick Start

New to Kraken-OS? Start here:

1. **[Getting Started Guide](GETTING_STARTED.md)** - Build and flash your first Kraken-OS application
2. **[Architecture Overview](ARCHITECTURE.md)** - Understand the system design
3. **[Service Development Guide](SERVICE_DEVELOPMENT.md)** - Create your first service
4. **[App Development Guide](APP_DEVELOPMENT.md)** - Build dynamic applications

---

## 📚 Core Documentation

### System Architecture
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - Complete system architecture with production features
- **[APPS_VS_SERVICES.md](APPS_VS_SERVICES.md)** - Understanding the difference
- **[MEMORY_MANAGEMENT.md](MEMORY_MANAGEMENT.md)** - SRAM vs PSRAM usage patterns

### Development Guides
- **[SERVICE_DEVELOPMENT.md](SERVICE_DEVELOPMENT.md)** - Complete service development guide
- **[APP_DEVELOPMENT.md](APP_DEVELOPMENT.md)** - Application development with examples
- **[PRODUCTION_FEATURES.md](PRODUCTION_FEATURES.md)** - Using watchdog, quotas, and monitoring

### API Reference
- **[API_REFERENCE.md](API_REFERENCE.md)** - Complete API documentation
- **[EVENT_BUS_API.md](EVENT_BUS_API.md)** - Event system usage
- **[PRODUCTION_API.md](PRODUCTION_API.md)** - Watchdog, quotas, and monitoring APIs

### Examples
- **[example_simple_app.c](example_simple_app.c)** - Simple application example
- **[example_service.c](example_service.c)** - Complete service example with production features
- **[example_production_integration.c](example_production_integration.c)** - Production features integration

---

## 🎯 Production Features

Kraken-OS includes enterprise-grade features:

### Fault Tolerance
- **Service Watchdog** - Automatic restart on failure
- **Handler Monitoring** - Timeout detection
- **Safe Mode** - Critical service protection

### Resource Management
- **Memory Pools** - Reduced fragmentation
- **Resource Quotas** - Per-service limits
- **Priority Queues** - Event prioritization

### Observability
- **Health Monitoring** - Real-time statistics
- **Performance Metrics** - Latency tracking
- **Heap Monitoring** - Fragmentation analysis

See **[PRODUCTION_FEATURES.md](PRODUCTION_FEATURES.md)** for details.

---

## 🏗️ System Components

### Core System Service
The heart of Kraken-OS, providing:
- Event bus with priority queues
- Service registration and lifecycle
- Memory pool management
- Watchdog monitoring
- Resource quota enforcement

### Hardware Services
- **Audio Service** - Audio playback and control
- **Bluetooth Service** - BLE connectivity
- **Display Service** - ST7789 LCD control
- **Network Service** - WiFi connectivity
- **Input Service** - Touch and button input

### Application Framework
- Dynamic app loading
- Lifecycle management
- Event subscription
- Resource isolation

---

## 📖 Documentation Structure

```
docs/
├── INDEX.md                          # This file
├── GETTING_STARTED.md               # Quick start guide
├── ARCHITECTURE.md                   # System architecture
├── SERVICE_DEVELOPMENT.md           # Service development
├── APP_DEVELOPMENT.md               # App development
├── PRODUCTION_FEATURES.md           # Production features guide
├── API_REFERENCE.md                 # Complete API reference
├── EVENT_BUS_API.md                 # Event system API
├── PRODUCTION_API.md                # Production features API
├── MEMORY_MANAGEMENT.md             # Memory usage guide
├── APPS_VS_SERVICES.md              # Architectural concepts
├── TROUBLESHOOTING.md               # Common issues
├── example_simple_app.c             # Simple app example
├── example_service.c                # Service example
└── example_production_integration.c # Production integration
```

---

## 🎓 Learning Path

### For Beginners
1. Read [GETTING_STARTED.md](GETTING_STARTED.md)
2. Understand [ARCHITECTURE.md](ARCHITECTURE.md)
3. Try [example_simple_app.c](example_simple_app.c)
4. Build your first app with [APP_DEVELOPMENT.md](APP_DEVELOPMENT.md)

### For Service Developers
1. Review [ARCHITECTURE.md](ARCHITECTURE.md)
2. Study [SERVICE_DEVELOPMENT.md](SERVICE_DEVELOPMENT.md)
3. Examine [example_service.c](example_service.c)
4. Integrate production features from [PRODUCTION_FEATURES.md](PRODUCTION_FEATURES.md)

### For System Integrators
1. Understand [ARCHITECTURE.md](ARCHITECTURE.md)
2. Learn [PRODUCTION_FEATURES.md](PRODUCTION_FEATURES.md)
3. Configure with [API_REFERENCE.md](API_REFERENCE.md)
4. Monitor with production APIs

---

## 🔧 Configuration

Kraken-OS is highly configurable via `idf.py menuconfig`:

```bash
Component config → System Service Configuration
```

Available options:
- Watchdog timeouts
- Memory pool sizes
- Resource quotas
- Priority queue depths
- Handler monitoring thresholds

---

## 🐛 Troubleshooting

See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) for common issues and solutions.

---

## 📊 System Statistics

Kraken-OS provides comprehensive runtime statistics:
- Service health and heartbeats
- Event processing metrics
- Memory pool usage
- Heap fragmentation
- Quota violations
- Handler execution times

Access via `system_service_get_stats()` or monitor logs.

---

## 🤝 Contributing

When developing for Kraken-OS:

1. **Follow the architecture** - Services for hardware, apps for features
2. **Use production features** - Register with watchdog, set quotas
3. **Handle errors properly** - Use specific error codes
4. **Document your code** - Doxygen-style comments
5. **Test thoroughly** - Include fault injection

---

## 📝 Version Information

- **Kraken-OS Version**: 1.0 (Production Ready)
- **ESP-IDF Version**: 5.5+
- **Target**: ESP32-S3
- **Features**: 4000+ lines of production code

---

## 🚀 What's New in Production Version

- ✅ Memory pool system (reduced fragmentation)
- ✅ Service watchdog (automatic restart)
- ✅ Priority event queues (CRITICAL/HIGH/NORMAL/LOW)
- ✅ Resource quotas (per-service limits)
- ✅ Handler monitoring (timeout detection)
- ✅ Heap monitoring (fragmentation tracking)
- ✅ Comprehensive error codes (50+ specific errors)
- ✅ Runtime log control (per-service levels)

---

## 📞 Support

For questions or issues:
1. Check [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
2. Review relevant documentation
3. Examine example code
4. Check system logs and statistics

---

**Happy coding with Kraken-OS!** 🦑
