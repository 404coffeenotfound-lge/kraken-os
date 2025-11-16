# Dynamic App Loading - Professional Fix Summary

## Problem Overview
Dynamic app loading was crashing with "Cache error / MMU entry fault" when trying to execute loaded apps.

## Root Causes Identified

### 1. **Incorrect Section Loading**
- **Issue**: Sections were being concatenated without preserving their ELF virtual address layout
- **Impact**: Relocations couldn't find the correct addresses to patch
- **Fix**: Implemented section mapping table that tracks ELF virtual address → loaded RAM address

### 2. **Incorrect Relocation Handling**  
- **Issue**: R_XTENSA_RELATIVE relocations weren't reading existing values when addend=0
- **Impact**: Literal pools (containing pointers to strings/data) weren't relocated
- **Fix**: Read value from location when addend is 0, then map through section table

### 3. **External Symbol Resolution**
- **Issue**: PLT/GOT relocations for external functions weren't being resolved
- **Impact**: Calls to libc functions (`malloc`, `free`, `strlen`, etc.) would fail
- **Fix**: Implemented `resolve_external_symbol()` that maps symbol names to firmware addresses

### 4. **Non-Executable Memory Allocation**
- **Issue**: Code allocated with `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT` isn't executable
- **Impact**: Instruction fetch from allocated RAM caused MMU fault
- **Fix**: Use `MALLOC_CAP_EXEC | MALLOC_CAP_INTERNAL` to allocate from IRAM

## Implementation Details

### Section Mapping System
```c
typedef struct {
    uint32_t elf_addr;       // Virtual address in ELF file
    void *loaded_addr;        // Actual address in RAM after loading  
    size_t size;
} section_mapping_t;
```

Each section (.text, .rodata, .data, etc.) is tracked individually, allowing relocations to correctly map ELF addresses to loaded addresses.

### Relocation Processing
1. **R_XTENSA_RELATIVE**: Base + addend relocations for PIC code
   - If addend=0, read ELF VA from the location itself
   - Map ELF VA through section table to get loaded address
   
2. **R_XTENSA_JMP_SLOT**: PLT entries for function calls
   - Look up symbol name in dynamic symbol table
   - Resolve to firmware function address via `resolve_external_symbol()`
   
3. **R_XTENSA_GLOB_DAT**: Global data relocations
   - Similar to JMP_SLOT but for data symbols

### Cache Synchronization
Critical for ESP32-S3 with separate I/D caches:
```c
Cache_WriteBack_All();          // Flush modified code/data from cache
Cache_Invalidate_ICache_All();  // Force CPU to refetch instructions
```

Applied after:
1. Loading sections to RAM
2. Applying all relocations

## Files Modified

### `/components/system/src/app_loader.c`
- **load_sections_hybrid()**: Complete rewrite with section mapping
- **app_loader_apply_relocations()**: Added symbol resolution and proper address mapping  
- **resolve_external_symbol()**: New function to map libc/ESP functions
- **map_elf_addr_to_loaded()**: Maps ELF virtual addresses to loaded RAM addresses

## Current Status

✅ **Working**:
- Section loading with proper address mapping
- Symbol resolution for external functions
- Cache synchronization
- Relocation application for most types

⚠️ **Known Issues**:
1. Code must fit in IRAM (~200KB) OR require CONFIG_SPIRAM_FETCH_INSTRUCTIONS
2. Some relocations log warnings for addresses 0x0, 0x1, 0x2 (special runtime values)
3. Entry point calculation may need adjustment based on app structure

## Testing

### Build and Flash
```bash
cd /Users/jayz/Development/kraken-os
. /Users/jayz/Development/kraken/esp-idf/export.sh
idf.py build flash monitor
```

### Expected Output
```
I (2061) app_loader:   Map: ELF 0x000003C4 -> RAM 0x40374000 (.text, 860 bytes)
I (2111) app_loader:   Map: ELF 0x00000720 -> RAM 0x3fcb8c5b (.rodata, 1045 bytes)
...
I (2161) app_loader: ✓ Applied 26 relocations
I (2161) app_loader: ✓ Caches synchronized after relocations
```

## Recommendations for Production

### 1. Enable PSRAM Execution (Recommended)
In `sdkconfig`:
```
CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y
CONFIG_SPIRAM_RODATA=y
```

This allows:
- Larger apps (megabytes instead of ~200KB)
- Execute code directly from PSRAM
- No IRAM pressure

### 2. XIP (Execute-In-Place) Alternative
For very large apps:
- Keep code in flash partition  
- Map flash to address space using `spi_flash_mmap()`
- Execute directly from flash (slower but no RAM needed)
- Still load .data/.bss to RAM

### 3. App Size Optimization
- Use `-Os` flag (already set in `build_pic_app.sh`)
- Strip debug symbols from production apps
- Use `--gc-sections` to remove unused code

## Architecture Notes

### Why PIC (Position Independent Code)?
- Apps can load at any address (no hardcoded addresses)
- Uses relative addressing and GOT/PLT for external references
- Compiler flag: `-fPIC -fpic`

### ELF Sections in PIC Apps
- `.text`: Executable code
- `.rodata`: Read-only data (strings, constants)
- `.data.rel.ro`: Relocated read-only data  
- `.got`: Global Offset Table (function pointers)
- `.plt`: Procedure Linkage Table (function call stubs)
- `.dynamic`: Dynamic linking information

### Memory Layout
```
Flash Partition "app_store":
┌─────────────────┐ 0x410000
│   hello.elf     │
├─────────────────┤ 0x420000  
│   goodbye.elf   │
└─────────────────┘

RAM After Loading:
┌─────────────────┐
│  .text (IRAM)   │ ← Executable code
├─────────────────┤
│  .rodata        │ ← Strings, constants
│  .got/.plt      │ ← Function pointers (relocated)
│  .data          │ ← Initialized data
├─────────────────┤
│  .bss           │ ← Zero-initialized data
└─────────────────┘
```

## API for Apps

Apps receive `app_context_t*` parameter with function pointers to:
- `register_service()`, `set_state()`, `heartbeat()`
- `post_event()`, `subscribe_event()`
- `malloc()`, `free()` (PSRAM-aware)
- Standard libc resolved via PLT/GOT

## Next Steps for Complete Solution

1. ✅ Fix section mapping
2. ✅ Fix relocations
3. ✅ Add symbol resolution  
4. ✅ Fix memory allocation
5. ⏳ Test app execution
6. ⏳ Handle edge cases in relocations
7. ⏳ Add app unloading/cleanup
8. ⏳ Add security (signature verification)

## Professional Completion Checklist

- [x] Section layout preservation
- [x] Comprehensive relocation handling
- [x] External symbol resolution
- [x] Cache synchronization
- [x] Proper memory allocation
- [x] Detailed logging
- [ ] Full app execution verification
- [ ] Error handling for all edge cases  
- [ ] Performance optimization
- [ ] Security hardening
- [ ] Documentation

## Contact
For questions or issues with dynamic app loading, refer to this document and the implementation in `components/system/src/app_loader.c`.

---
**Last Updated**: Nov 11, 2025
**Status**: Core functionality complete, testing in progress
