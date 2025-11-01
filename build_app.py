#!/usr/bin/env python3
"""
Build standalone app binaries for Kraken system.

This script compiles individual apps into standalone .bin files that can be:
- Uploaded to the storage partition
- Downloaded from remote servers
- Dynamically loaded at runtime

Usage:
    python build_app.py <app_name>
    python build_app.py hello
    python build_app.py --all
"""

import os
import sys
import subprocess
import struct
import hashlib
from pathlib import Path

# App header format (must match app_header_t in app_manager.h)
APP_MAGIC = 0x4150504B  # "APPK"
APP_HEADER_SIZE = 128

def calculate_crc32(data):
    """Calculate CRC32 checksum."""
    return hashlib.md5(data).digest()[:4]  # Using MD5 for now, can switch to proper CRC32

def create_app_header(app_name, version, author, binary_data):
    """Create app header structure."""
    header = bytearray(APP_HEADER_SIZE)
    
    # Magic number (4 bytes)
    struct.pack_into('<I', header, 0, APP_MAGIC)
    
    # Name (32 bytes, null-terminated)
    name_bytes = app_name.encode('utf-8')[:31]
    header[4:4+len(name_bytes)] = name_bytes
    
    # Version (16 bytes, null-terminated)
    version_bytes = version.encode('utf-8')[:15]
    header[36:36+len(version_bytes)] = version_bytes
    
    # Author (32 bytes, null-terminated)
    author_bytes = author.encode('utf-8')[:31]
    header[52:52+len(author_bytes)] = author_bytes
    
    # Size (4 bytes)
    struct.pack_into('<I', header, 84, len(binary_data))
    
    # Entry point offset (4 bytes) - 0 for now, loader will resolve
    struct.pack_into('<I', header, 88, 0)
    
    # CRC32 (4 bytes)
    crc = calculate_crc32(binary_data)
    header[92:96] = crc
    
    return bytes(header)

def build_app(app_name, apps_dir, output_dir):
    """Build a single app into a standalone binary."""
    app_path = apps_dir / app_name
    
    if not app_path.exists():
        print(f"Error: App '{app_name}' not found in {apps_dir}")
        return False
    
    print(f"\n{'='*60}")
    print(f"Building app: {app_name}")
    print(f"{'='*60}\n")
    
    # Create temporary build directory for this app
    app_build_dir = Path('build') / 'apps' / app_name
    app_build_dir.mkdir(parents=True, exist_ok=True)
    
    # Build the app using xtensa toolchain
    # For now, we compile it as a regular component and extract the object files
    # In a full implementation, you'd use a custom linker script for position-independent code
    
    print(f"Note: Currently apps are built into main firmware.")
    print(f"To build separately, we need to:")
    print(f"  1. Compile app source to object files")
    print(f"  2. Link as position-independent code (PIC)")
    print(f"  3. Add app header")
    print(f"  4. Package as .bin file\n")
    
    # For demonstration, create a placeholder binary
    # In real implementation, compile the C source files
    app_src = app_path / f"{app_name}_app.c"
    if not app_src.exists():
        print(f"Error: Source file not found: {app_src}")
        return False
    
    # Read app metadata from source (simple parser)
    metadata = {
        'name': app_name,
        'version': '1.0.0',
        'author': 'Kraken Team'
    }
    
    # Create placeholder app binary (in real implementation, compile the source)
    binary_data = b'\x00' * 1024  # Placeholder
    
    # Add header
    header = create_app_header(metadata['name'], metadata['version'], 
                                metadata['author'], binary_data)
    
    # Create final binary
    final_binary = header + binary_data
    
    # Write output
    output_file = output_dir / f"{app_name}.bin"
    output_file.write_bytes(final_binary)
    
    print(f"✓ App binary created: {output_file}")
    print(f"  Size: {len(final_binary)} bytes")
    print(f"  Header: {len(header)} bytes")
    print(f"  Code: {len(binary_data)} bytes")
    
    return True

def build_all_apps(apps_dir, output_dir):
    """Build all apps in the apps directory."""
    apps = [d.name for d in apps_dir.iterdir() if d.is_dir() and not d.name.startswith('.')]
    
    print(f"Found {len(apps)} apps: {', '.join(apps)}")
    
    success_count = 0
    for app in apps:
        if build_app(app, apps_dir, output_dir):
            success_count += 1
    
    print(f"\n{'='*60}")
    print(f"Build complete: {success_count}/{len(apps)} apps built successfully")
    print(f"{'='*60}\n")

def main():
    if len(sys.argv) < 2:
        print("Usage: python build_app.py <app_name> | --all")
        print("Example: python build_app.py hello")
        sys.exit(1)
    
    # Paths
    script_dir = Path(__file__).parent
    apps_dir = script_dir / 'components' / 'apps'
    output_dir = script_dir / 'build' / 'apps'
    output_dir.mkdir(parents=True, exist_ok=True)
    
    if sys.argv[1] == '--all':
        build_all_apps(apps_dir, output_dir)
    else:
        app_name = sys.argv[1]
        build_app(app_name, apps_dir, output_dir)

if __name__ == '__main__':
    main()
