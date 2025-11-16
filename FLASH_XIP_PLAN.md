# Flash XIP (Execute In Place) Implementation Plan

## Goal
Execute dynamic app code directly from flash memory instead of copying to RAM.
This is the standard ESP-IDF approach and how the main firmware works.

## Implementation Steps

### 1. Memory Map Flash Region
Instead of copying ELF to RAM, use spi_flash_mmap() to map flash to address space:
- Map flash partition containing app ELF
- Get virtual address pointer for execution
- Code stays in flash, executes via instruction cache

### 2. Modify app_loader.c
Key changes:
- Don't allocate RAM for code segment
- Use spi_flash_mmap() with SPI_FLASH_MMAP_INST flag
- Store mmap handle for cleanup
- Apply relocations to mapped memory (read-only issue needs handling)

### 3. Handle Relocations
Challenge: Flash is read-only, can't modify in-place
Solutions:
a) Build apps with no relocations needed (fully PIC)
b) Copy to RAM page-by-page, apply relocations, write back
c) Use trampolines/GOT for external references

### 4. Partition Layout
Apps stored in flash partition:
- Partition: app_store (0x410000)
- App 1 at offset 0
- App 2 at offset 0x10000 (64KB aligned for mmap)

## Code Changes Required

### app_loader.c - load_from_partition()
```c
esp_err_t app_loader_load_from_partition(const char *partition_label, 
                                          size_t offset, 
                                          loaded_app_t *out_app)
{
    // 1. Find partition
    const esp_partition_t *partition = esp_partition_find_first(...);
    
    // 2. Memory map the flash region (must be 64KB aligned)
    size_t aligned_offset = offset & ~(0xFFFF);  // 64KB align
    const void *mapped_ptr;
    spi_flash_mmap_handle_t handle;
    
    esp_err_t ret = spi_flash_mmap(
        partition->address + aligned_offset,
        partition->size - aligned_offset,
        SPI_FLASH_MMAP_INST,  // For instruction fetch
        &mapped_ptr,
        &handle
    );
    
    // 3. ELF is now accessible at mapped_ptr
    uint8_t *elf_data = (uint8_t*)mapped_ptr + (offset - aligned_offset);
    
    // 4. Parse ELF headers
    // 5. Set code_segment to mapped address
    // 6. Allocate RAM only for data/bss
    // 7. Handle relocations (challenge!)
    
    out_app->code_segment = mapped_code_addr;
    out_app->mmap_handle = handle;
    out_app->code_in_flash = true;
}
```

### Relocation Handling (Complex Part)
Option A - No Relocations (Cleanest):
- Build apps with -fPIE -fvisibility=hidden
- All code is position-independent
- Only data needs relocation

Option B - GOT/PLT (Industry Standard):
- Use Global Offset Table
- Code references data through GOT
- GOT is in RAM, can be relocated

Option C - Hybrid:
- Small code: copy to IRAM, apply relocations
- Large code: keep in flash, use trampolines

## Testing Plan
1. Test with tiny app (< 1KB) in IRAM first
2. Test with flash mapping (no relocations)
3. Add relocation support
4. Test with real apps

## Expected Benefits
✅ No IRAM usage for app code
✅ Scalable - limited only by flash size
✅ Fast execution via instruction cache
✅ Standard ESP32 approach
✅ Works with any app size

## Challenges
⚠️ Flash is read-only - can't apply relocations in-place
⚠️ Must handle 64KB alignment for mmap
⚠️ Need different relocation strategy
⚠️ Debugging is harder (code in flash not RAM)

## Alternative: Hybrid Approach
Small apps (< available IRAM): Copy to IRAM, apply relocations
Large apps: Use flash XIP with careful GOT handling
