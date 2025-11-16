# Flash XIP Issue - Root Cause Analysis

## Problem
`spi_flash_mmap(SPI_FLASH_MMAP_INST)` maps flash for instruction fetch, but:
- It expects LINKED/PROCESSED code (like ESP-IDF does for main app)
- ELF file format includes headers, not just raw code
- The .text section WITHIN an ELF file cannot be directly executed

## Current Crash
```
PC: 0x428003c4 (entry point in mapped flash)
Memory dump: 00000000 00000000 00000000
Exception: IllegalInstruction
```

The mapped address points into the ELF file structure, not executable code.

## Why Standard XIP Doesn't Work Here
ESP-IDF's Flash XIP works because:
1. Bootloader links the app with specific flash addresses
2. Code is placed at known offsets
3. MMU maps those exact regions
4. No ELF file - just raw binary segments

Our case:
1. We have ELF files in flash
2. Need to extract .text and apply relocations  
3. Can't execute ELF format directly

## Solutions

### Option A: Copy .text to PSRAM (RECOMMENDED)
After loading and relocating, copy .text to PSRAM
- ✅ Simple and reliable
- ✅ PSRAM execution enabled (`CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y`)
- ✅ Relocations already applied
- ❌ Uses PSRAM (but we have 8MB!)

### Option B: Extract raw .text to separate flash region
- Build process extracts .text section
- Flash raw code separately
- More complex build system

### Option C: In-place execution from ELF
- Parse ELF, map only .text section offset
- Very complex with relocations

## Recommendation
**Switch to PSRAM execution** - we already have it enabled!
Copy relocated .text to PSRAM and execute from there.

Code change: ~5 lines in `load_sections_hybrid()`
