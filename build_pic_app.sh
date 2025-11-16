#!/bin/bash
# Build script for creating Position-Independent Code (PIC) apps
# These apps can be loaded dynamically at runtime

set -e

APP_NAME="$1"
if [ -z "$APP_NAME" ]; then
    echo "Usage: $0 <app_name>"
    echo "Example: $0 hello"
    exit 1
fi

APP_DIR="components/apps/$APP_NAME"
BUILD_DIR="build/apps/$APP_NAME"
OUTPUT_DIR="build/app_binaries"

if [ ! -d "$APP_DIR" ]; then
    echo "Error: App directory $APP_DIR not found"
    exit 1
fi

# Check if main project has been built (need sdkconfig.h)
if [ ! -f "build/config/sdkconfig.h" ]; then
    echo "Error: sdkconfig.h not found. Please build the main project first:"
    echo "  idf.py build"
    echo ""
    echo "This generates the necessary configuration files."
    exit 1
fi

echo "============================================"
echo "Building PIC app: $APP_NAME"
echo "============================================"

# Create build directories
mkdir -p "$BUILD_DIR"
mkdir -p "$OUTPUT_DIR"

# Compiler settings for ESP32-S3
CC="xtensa-esp32s3-elf-gcc"
AR="xtensa-esp32s3-elf-ar"
LD="xtensa-esp32s3-elf-ld"
OBJCOPY="xtensa-esp32s3-elf-objcopy"

# Find IDF path
if [ -z "$IDF_PATH" ]; then
    # Try to detect from build directory
    if [ -f "build/project_description.json" ]; then
        IDF_PATH=$(python3 -c "import json; print(json.load(open('build/project_description.json'))['idf_path'])" 2>/dev/null)
        if [ -n "$IDF_PATH" ] && [ -d "$IDF_PATH" ]; then
            echo "Detected IDF_PATH from build: $IDF_PATH"
            export IDF_PATH
        fi
    fi
fi

if [ -z "$IDF_PATH" ] || [ ! -d "$IDF_PATH" ]; then
    echo "Error: IDF_PATH not set or invalid. Options:"
    echo "  1. Run 'source export.sh' from ESP-IDF directory"
    echo "  2. Build main project first with 'idf.py build' to generate project_description.json"
    exit 1
fi

# Add ESP-IDF tools to PATH if not already there
if ! command -v xtensa-esp32s3-elf-gcc &> /dev/null; then
    # Find toolchain in .espressif
    TOOLCHAIN_PATH=$(find ~/.espressif/tools/xtensa-esp-elf -name "xtensa-esp32s3-elf-gcc" 2>/dev/null | head -1)
    if [ -n "$TOOLCHAIN_PATH" ]; then
        TOOLCHAIN_DIR=$(dirname "$TOOLCHAIN_PATH")
        echo "Adding toolchain to PATH: $TOOLCHAIN_DIR"
        export PATH="$TOOLCHAIN_DIR:$PATH"
    else
        echo "Error: xtensa-esp32s3-elf-gcc not found. Please run 'source export.sh' from ESP-IDF"
        exit 1
    fi
fi

# Compiler flags for PIC
CFLAGS="-fPIC -fpic"
CFLAGS="$CFLAGS -mlongcalls"                    # Required for Xtensa  
CFLAGS="$CFLAGS -mtext-section-literals"        # Put literals in .text (accessible via I-cache)
CFLAGS="$CFLAGS -Os"                            # Optimize for size
CFLAGS="$CFLAGS -Wall"
CFLAGS="$CFLAGS -nostdlib"                      # No standard library (we provide APIs)
CFLAGS="$CFLAGS -ffunction-sections"            # Separate sections for each function
CFLAGS="$CFLAGS -fdata-sections"
CFLAGS="$CFLAGS -DESP_PLATFORM"                 # ESP platform define
CFLAGS="$CFLAGS -DIDF_VER=\"v5.5.1\""          # IDF version
CFLAGS="$CFLAGS -DCONFIG_IDF_TARGET_ESP32S3=1" # Target chip

# Include paths
INCLUDES="-I$IDF_PATH/components/freertos/FreeRTOS-Kernel/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/freertos/esp_additions/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/freertos/FreeRTOS-Kernel/portable/xtensa/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/freertos/config/xtensa/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/freertos/config/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/freertos/config/include/freertos"
INCLUDES="$INCLUDES -I$IDF_PATH/components/freertos/FreeRTOS-Kernel/portable/xtensa/include/freertos"
INCLUDES="$INCLUDES -I$IDF_PATH/components/esp_common/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/esp_system/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/esp_hw_support/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/esp_rom/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/esp_rom/include/esp32s3"
INCLUDES="$INCLUDES -I$IDF_PATH/components/xtensa/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/xtensa/esp32s3/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/soc/esp32s3/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/soc/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/soc/esp32s3"
INCLUDES="$INCLUDES -I$IDF_PATH/components/soc/esp32s3/register"
INCLUDES="$INCLUDES -I$IDF_PATH/components/hal/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/hal/esp32s3/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/hal/platform_port/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/log/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/heap/include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/newlib/platform_include"
INCLUDES="$INCLUDES -I$IDF_PATH/components/esp_timer/include"
INCLUDES="$INCLUDES -Icomponents/system/include"
INCLUDES="$INCLUDES -Ibuild/config"  # For sdkconfig.h and FreeRTOSConfig.h
INCLUDES="$INCLUDES -I$APP_DIR"

# Compile app source files
echo "Compiling $APP_NAME source files..."
for src in "$APP_DIR"/*.c; do
    if [ -f "$src" ]; then
        obj="$BUILD_DIR/$(basename ${src%.c}.o)"
        echo "  CC: $src -> $obj"
        $CC $CFLAGS $INCLUDES -c "$src" -o "$obj"
    fi
done

# Link into shared object (PIC executable)
echo "Linking PIC binary..."
LDFLAGS="-shared -fPIC"
LDFLAGS="$LDFLAGS -nostdlib"
LDFLAGS="$LDFLAGS -Wl,--gc-sections"            # Remove unused sections

$CC $LDFLAGS -o "$BUILD_DIR/${APP_NAME}.elf" "$BUILD_DIR"/*.o

# Create binary output
echo "Copying ELF to output..."
cp "$BUILD_DIR/${APP_NAME}.elf" "$OUTPUT_DIR/${APP_NAME}.elf"

# Also create a raw binary (for reference, but we'll flash the ELF)
echo "Creating raw binary..."
$OBJCOPY -O binary "$BUILD_DIR/${APP_NAME}.elf" "$OUTPUT_DIR/${APP_NAME}.bin"

# Show info
echo ""
echo "============================================"
echo "Build successful!"
echo "============================================"
echo "ELF file: $BUILD_DIR/${APP_NAME}.elf"
echo "Binary:   $OUTPUT_DIR/${APP_NAME}.elf (for flashing)"
echo "Raw bin:  $OUTPUT_DIR/${APP_NAME}.bin (for reference)"
echo ""

# Show size
xtensa-esp32s3-elf-size "$BUILD_DIR/${APP_NAME}.elf"

echo ""
echo "To flash this app to partition 'app_store' at offset 0:"
echo "  esptool.py --port /dev/ttyUSB0 write_flash 0x410000 $OUTPUT_DIR/${APP_NAME}.elf"
echo ""
echo "Or use the Makefile:"
echo "  make -f Makefile.apps flash-app APP=${APP_NAME}"
