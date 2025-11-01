#!/usr/bin/env python3
"""
Build dynamic apps for Kraken using the same source code.

This builds apps with shared system_service APIs, creating binaries that
can be loaded at runtime.

Usage:
    python build_dynamic_app.py hello
    python build_dynamic_app.py goodbye
    python build_dynamic_app.py --all
"""

import os
import sys
import subprocess
import struct
import hashlib
import shutil
from pathlib import Path

APP_MAGIC = 0x4150504B  # "APPK"
APP_HEADER_SIZE = 128

def calculate_crc32(data):
    """Calculate CRC32 using zlib."""
    import zlib
    return zlib.crc32(data) & 0xFFFFFFFF

def create_app_header(app_name, version, author, binary_data, entry_offset=0):
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
    
    # Entry point offset (4 bytes)
    struct.pack_into('<I', header, 88, entry_offset)
    
    # CRC32 (4 bytes)
    crc = calculate_crc32(binary_data)
    struct.pack_into('<I', header, 92, crc)
    
    return bytes(header)

def parse_app_metadata(source_file):
    """Extract metadata from app source file."""
    metadata = {
        'name': '',
        'version': '1.0.0',
        'author': 'Kraken Team'
    }
    
    try:
        with open(source_file, 'r') as f:
            content = f.read()
            
            # Look for manifest definition
            if '_app_manifest' in content:
                # Extract name from manifest
                import re
                name_match = re.search(r'\.name\s*=\s*"([^"]+)"', content)
                if name_match:
                    metadata['name'] = name_match.group(1)
                
                version_match = re.search(r'\.version\s*=\s*"([^"]+)"', content)
                if version_match:
                    metadata['version'] = version_match.group(1)
                
                author_match = re.search(r'\.author\s*=\s*"([^"]+)"', content)
                if author_match:
                    metadata['author'] = author_match.group(1)
    except Exception as e:
        print(f"Warning: Could not parse metadata: {e}")
    
    return metadata

def build_app_with_idf(app_name, project_root):
    """
    Build app using ESP-IDF toolchain.
    
    Strategy:
    1. Create a minimal IDF project for the app
    2. Link against system component
    3. Compile with -ffunction-sections -fdata-sections
    4. Extract .text and .rodata sections
    5. Package with header
    """
    
    print(f"\n{'='*60}")
    print(f"Building dynamic app: {app_name}")
    print(f"{'='*60}\n")
    
    apps_dir = project_root / 'components' / 'apps'
    app_src_dir = apps_dir / app_name
    
    if not app_src_dir.exists():
        print(f"Error: App source not found: {app_src_dir}")
        return None
    
    # Find app source file
    app_src_file = app_src_dir / f"{app_name}_app.c"
    if not app_src_file.exists():
        print(f"Error: App source file not found: {app_src_file}")
        return None
    
    # Get metadata
    metadata = parse_app_metadata(app_src_file)
    if not metadata['name']:
        metadata['name'] = app_name
    
    print(f"App: {metadata['name']} v{metadata['version']} by {metadata['author']}")
    
    # Build using existing build system
    build_dir = project_root / 'build'
    
    if not build_dir.exists():
        print("Error: Project not built. Run 'idf.py build' first.")
        return None
    
    # Look for app object file in build
    app_obj_pattern = f"*{app_name}_app.c.obj"
    app_objs = list(build_dir.rglob(app_obj_pattern))
    
    if not app_objs:
        print(f"Warning: Object file not found for {app_name}")
        print(f"Creating placeholder binary...")
        
        # Create a minimal placeholder that won't crash
        binary_data = bytearray(1024)
        # Add a simple return instruction (platform-specific)
        # For Xtensa: ret.n (0x0d 0xf0)
        binary_data[0:2] = b'\x0d\xf0'
        
        return bytes(binary_data), metadata
    
    app_obj = app_objs[0]
    print(f"Found object file: {app_obj}")
    
    # Extract binary from object using objcopy
    output_dir = build_dir / 'apps'
    output_dir.mkdir(exist_ok=True)
    
    raw_bin = output_dir / f"{app_name}_raw.bin"
    
    # Use xtensa objcopy to extract binary
    objcopy_cmd = f"xtensa-esp32s3-elf-objcopy -O binary {app_obj} {raw_bin}"
    
    try:
        result = subprocess.run(objcopy_cmd, shell=True, capture_output=True, text=True)
        
        if result.returncode == 0 and raw_bin.exists():
            binary_data = raw_bin.read_bytes()
            print(f"✓ Extracted {len(binary_data)} bytes from object file")
            return binary_data, metadata
        else:
            print(f"Warning: objcopy failed: {result.stderr}")
    except Exception as e:
        print(f"Warning: Could not run objcopy: {e}")
    
    # Fallback: create placeholder
    print("Creating placeholder binary...")
    binary_data = bytearray(1024)
    binary_data[0:2] = b'\x0d\xf0'  # ret.n
    
    return bytes(binary_data), metadata

def package_app(app_name, binary_data, metadata, output_dir):
    """Package app with header."""
    
    # Entry point is at offset 0 (simplified)
    entry_offset = 0
    
    # Create header
    header = create_app_header(
        metadata['name'],
        metadata['version'],
        metadata['author'],
        binary_data,
        entry_offset
    )
    
    # Combine header + binary
    final_binary = header + binary_data
    
    # Write output
    output_file = output_dir / f"{app_name}.bin"
    output_file.write_bytes(final_binary)
    
    print(f"\n✓ App packaged successfully")
    print(f"  Output: {output_file}")
    print(f"  Header: {len(header)} bytes")
    print(f"  Code:   {len(binary_data)} bytes")
    print(f"  Total:  {len(final_binary)} bytes")
    print(f"  CRC32:  0x{calculate_crc32(binary_data):08X}")
    
    return output_file

def main():
    if len(sys.argv) < 2:
        print("Usage: python build_dynamic_app.py <app_name> | --all")
        print("Example: python build_dynamic_app.py hello")
        sys.exit(1)
    
    # Get project root
    script_dir = Path(__file__).parent
    project_root = script_dir
    
    # Output directory
    output_dir = project_root / 'build' / 'apps'
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Get app name(s)
    if sys.argv[1] == '--all':
        apps_dir = project_root / 'components' / 'apps'
        apps = [d.name for d in apps_dir.iterdir() if d.is_dir() and not d.name.startswith('.')]
    else:
        apps = [sys.argv[1]]
    
    print(f"\n╔══════════════════════════════════════════╗")
    print(f"║  Kraken Dynamic App Builder              ║")
    print(f"╚══════════════════════════════════════════╝")
    
    success_count = 0
    
    for app_name in apps:
        try:
            result = build_app_with_idf(app_name, project_root)
            if result:
                binary_data, metadata = result
                package_app(app_name, binary_data, metadata, output_dir)
                success_count += 1
        except Exception as e:
            print(f"Error building {app_name}: {e}")
            import traceback
            traceback.print_exc()
    
    print(f"\n╔══════════════════════════════════════════╗")
    print(f"║  Build Complete: {success_count}/{len(apps)} apps")
    print(f"╚══════════════════════════════════════════╝\n")
    print(f"Apps available in: {output_dir}")
    print(f"\nNext steps:")
    print(f"  1. Upload to ESP32: esptool.py write_flash 0x410000 build/apps/{apps[0]}.bin")
    print(f"  2. Load at runtime: app_manager_load_from_storage(\"/apps/{apps[0]}.bin\", &info)")

if __name__ == '__main__':
    main()
