#!/bin/bash
# Extract and package individual apps from Kraken firmware build
#
# This script extracts compiled app code and creates standalone .bin files
# that can be uploaded to storage or downloaded at runtime.
#
# Usage:
#   ./extract_apps.sh              # Extract all apps
#   ./extract_apps.sh hello        # Extract specific app
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
APPS_OUTPUT_DIR="$BUILD_DIR/apps"
COMPONENTS_DIR="$SCRIPT_DIR/components/apps"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

mkdir -p "$APPS_OUTPUT_DIR"

echo -e "${GREEN}╔══════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║     Kraken App Extraction Tool          ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════╝${NC}"
echo ""

# Check if build exists
if [ ! -f "$BUILD_DIR/kraken.elf" ]; then
    echo -e "${RED}Error: kraken.elf not found. Please run 'idf.py build' first.${NC}"
    exit 1
fi

# Function to extract app symbols and create standalone binary
extract_app() {
    local APP_NAME=$1
    echo -e "${YELLOW}Extracting app: $APP_NAME${NC}"
    
    # Check if app source exists
    if [ ! -d "$COMPONENTS_DIR/$APP_NAME" ]; then
        echo -e "${RED}  Error: App directory not found: $COMPONENTS_DIR/$APP_NAME${NC}"
        return 1
    fi
    
    # For ESP32, we need to extract the compiled object files
    # Find the app's object files in the build directory
    APP_OBJ_DIR="$BUILD_DIR/esp-idf/apps"
    
    if [ -d "$APP_OBJ_DIR" ]; then
        # Find object files for this app
        APP_OBJ=$(find "$APP_OBJ_DIR" -name "${APP_NAME}_app.c.obj" 2>/dev/null | head -1)
        
        if [ -n "$APP_OBJ" ]; then
            # Extract binary from object file
            xtensa-esp32s3-elf-objcopy -O binary "$APP_OBJ" "$APPS_OUTPUT_DIR/${APP_NAME}_raw.bin"
            
            # Get size
            SIZE=$(stat -f%z "$APPS_OUTPUT_DIR/${APP_NAME}_raw.bin" 2>/dev/null || stat -c%s "$APPS_OUTPUT_DIR/${APP_NAME}_raw.bin")
            
            # Add app header (for now, just copy raw binary)
            cp "$APPS_OUTPUT_DIR/${APP_NAME}_raw.bin" "$APPS_OUTPUT_DIR/${APP_NAME}.bin"
            
            echo -e "${GREEN}  ✓ Created: $APPS_OUTPUT_DIR/${APP_NAME}.bin ($SIZE bytes)${NC}"
        else
            echo -e "${YELLOW}  ! Object file not found for $APP_NAME${NC}"
            echo -e "${YELLOW}    Creating placeholder for demonstration${NC}"
            
            # Create a placeholder that includes the manifest
            echo "Kraken App: $APP_NAME" > "$APPS_OUTPUT_DIR/${APP_NAME}.bin"
            echo -e "${GREEN}  ✓ Placeholder created: $APPS_OUTPUT_DIR/${APP_NAME}.bin${NC}"
        fi
    else
        echo -e "${YELLOW}  ! Build artifacts not found${NC}"
        echo -e "${YELLOW}    Creating placeholder for demonstration${NC}"
        echo "Kraken App: $APP_NAME" > "$APPS_OUTPUT_DIR/${APP_NAME}.bin"
        echo -e "${GREEN}  ✓ Placeholder created: $APPS_OUTPUT_DIR/${APP_NAME}.bin${NC}"
    fi
    
    echo ""
}

# Main logic
if [ -n "$1" ]; then
    # Extract specific app
    extract_app "$1"
else
    # Extract all apps
    echo "Scanning for apps in: $COMPONENTS_DIR"
    echo ""
    
    for APP_DIR in "$COMPONENTS_DIR"/*; do
        if [ -d "$APP_DIR" ]; then
            APP_NAME=$(basename "$APP_DIR")
            extract_app "$APP_NAME"
        fi
    done
fi

echo -e "${GREEN}╔══════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║          Extraction Complete             ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════╝${NC}"
echo ""
echo "App binaries available in: $APPS_OUTPUT_DIR"
echo ""
echo "Next steps:"
echo "  1. Upload to ESP32 storage: esptool.py write_flash 0x410000 build/apps/hello.bin"
echo "  2. Or serve via HTTP for remote download"
echo ""
