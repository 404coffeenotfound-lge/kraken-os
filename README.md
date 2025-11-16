# Kraken OS

ESP32-S3 based operating system with modular services and static app loading.

## Features

- **Service Architecture**: Modular services (audio, bluetooth, display, network, input)
- **Event System**: Pub/sub event bus for inter-service communication
- **App Framework**: Static app loading with simple API
- **LVGL Display**: 240x320 ST7789 LCD support
- **PSRAM**: 8MB external RAM support

## Quick Start

```bash
# Build
cd kraken
idf.py build

# Flash and monitor
idf.py flash monitor
```

## Documentation

- [Static App Loading](STATIC_APP_LOADING.md) - App development guide
- [Implementation Summary](IMPLEMENTATION_SUMMARY.md) - System architecture
- [Quick Reference](QUICK_REFERENCE.md) - API reference

## System Services

- **Audio Service**: Audio playback control
- **Bluetooth Service**: BLE communication
- **Display Service**: LCD/LVGL graphics
- **Network Service**: WiFi connectivity
- **Input Service**: Button/touch input

## Apps

Located in `apps/` directory:
- `hello` - Hello world demonstration
- `goodbye` - Secondary app example

## Requirements

- ESP-IDF v5.5+
- ESP32-S3 with 8MB PSRAM
- ST7789 240x320 LCD (optional)

## License

See LICENSE file
