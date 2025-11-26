# Virtual Memory Simulator

A lightweight virtual memory management system implementation in C that simulates page tables, address translation, and page fault handling.

## Overview

This simulator implements a two-level page table architecture with demand paging. It provides a software-based virtual memory system that manages a 1 MB RAM space with 4 KB pages, featuring automatic page fault handling and access permission controls.

## Architecture

### Memory Layout
- **RAM Size**: 1 MB (1,048,576 bytes)
- **Page Size**: 4 KB (4,096 bytes)
- **Physical Pages**: 256 pages
- **Virtual Address Space**: 1 MB (20-bit addresses)

### Page Table Structure
- **Two-level page table** with 16 entries at each level
- **L1 Table**: 16 entries pointing to L2 tables
- **L2 Table**: 16 entries per table
- **Total Virtual Pages**: 256 (16 × 16)

### Address Translation
Virtual addresses are decomposed as follows:
```
| L1 Index (4 bits) | L2 Index (4 bits) | Offset (12 bits) |
|     [19:16]       |     [15:12]       |     [11:0]       |
```

## Features

- **Demand Paging**: Pages are allocated on first access (automatic page fault handling)
- **Permission Control**: Read, write, and valid flags per page
- **Statistics Tracking**: Page faults, reads, writes, and translation failures
- **Memory Management**: Physical page allocation and deallocation
- **Error Handling**: Bounds checking and permission validation

## API Reference

### Initialization
```c
void init_vm(void)
```
Initializes the virtual memory system. Must be called before any other operations.

### Page Mapping
```c
int map_page(uint16_t virt_page, uint16_t phys_page, uint8_t flags)
```
Manually maps a virtual page to a physical page with specified permissions.

**Parameters:**
- `virt_page`: Virtual page number (0-255)
- `phys_page`: Physical page number (0-255)
- `flags`: Permission flags (PTE_READ, PTE_WRITE, PTE_VALID)

**Returns:** 0 on success, -1 on error

### Memory Access
```c
int read_vmem(uint32_t vaddr, uint8_t *out)
int write_vmem(uint32_t vaddr, uint8_t val)
```
Read from or write to virtual memory. Automatically handles page faults.

**Parameters:**
- `vaddr`: Virtual address
- `out`/`val`: Output buffer or value to write

**Returns:** 0 on success, -1 on error

### Page Unmapping
```c
int unmap_page(uint16_t virt_page)
```
Unmaps a virtual page and frees the associated physical page.

### Statistics
```c
void print_stats(void)
```
Prints comprehensive statistics including page faults, read/write counts, translation failures, and physical memory usage.

### Cleanup
```c
void free_pages(void)
```
Frees all allocated L2 page tables. Call before program termination.

## Permission Flags

- `PTE_VALID` (0x01): Page table entry is valid
- `PTE_WRITE` (0x02): Write permission
- `PTE_READ` (0x04): Read permission

Common combinations:
```c
uint8_t READ_ONLY  = PTE_READ;
uint8_t WRITE_ONLY = PTE_WRITE;
uint8_t READ_WRITE = PTE_READ | PTE_WRITE;
```

## Example Usage

```c
// your test(s) file: test.c
#include "pages.c"
// I didn't implement pages.h header. I will do it in future
int main() {
    // Initialize virtual memory system
    init_vm();
    
    // Manually map a page with read-write permissions
    map_page(0x00, 42, PTE_READ | PTE_WRITE);
    
    // Write to virtual memory (triggers page fault if not mapped)
    write_vmem(0x000510, 0xAB);
    
    // Read from virtual memory
    uint8_t value;
    read_vmem(0x000510, &value);
    printf("Read value: 0x%02X\n", value);
    
    // Access a new page (triggers automatic page fault handling)
    write_vmem(0x010000, 0xDE);
    read_vmem(0x010000, &value);
    printf("Read value: 0x%02X\n", value);
    
    // Display statistics
    print_stats();
    
    // Cleanup
    free_pages();
    return 0;
}
```

## Statistics Output

The `print_stats()` function displays:
- **Page faults**: Number of times page fault handler was invoked
- **Reads**: Successful read operations
- **Writes**: Successful write operations
- **Trans fails**: Translation failures (permission denied, invalid pages)
- **PHY used**: Physical pages currently allocated

## Error Handling

The simulator handles several error conditions:
- Out-of-bounds virtual/physical addresses
- Permission violations (read/write on restricted pages)
- Out of physical memory
- Invalid page table entries
- Memory allocation failures

Error codes:
- `0`: Success
- `-1`: General failure (invalid address, out of memory, etc.)
- `-2`: Permission denied

## Implementation Details

### Page Fault Handler
When a translation fails due to an unmapped page, the page fault handler:
1. Increments the page fault counter
2. Allocates a new physical page
3. Maps the virtual page to the physical page with read-write permissions
4. Retries the translation

### Physical Memory Management
It uses a byte-per-page allocation table (one bool/byte per page) (`phys_pages_used` array). The allocator performs a linear search for free pages.

## Limitations

- Fixed virtual address space (1 MB)
- No page replacement policy (no swapping)
- Simple linear allocation for physical pages
- No TLB (Translation Lookaside Buffer) simulation
- Single-threaded operation
## Future Improvements
### Disk Simulation and Page Swapping
- Implement a simulated disk/swap space for page replacement
- Add page replacement algorithms (LRU, FIFO, Clock, etc.)
- Support swapping pages between RAM and disk when physical memory is full
- Track dirty bits to optimize write-back operations
- Implement asynchronous disk I/O simulation with latency modeling
### Translation Lookaside Buffer (TLB)
- Add TLB cache to speed up address translation
- Implement TLB hit/miss tracking and statistics
- Support configurable TLB size and associativity
- Add TLB invalidation on page table updates
- Track TLB hit rate for performance analysis
### Bitmap Optimizations
- Replace linear physical page allocation with bitmap-based allocation
- Implement efficient bit manipulation for faster free page search
- Add buddy system or slab allocator for better memory management
- Support contiguous page allocation for large memory blocks
- Track fragmentation statistics
## Author

RK

## Compilation

```bash
gcc -o test test.c -std=c99 -Wall
```

## License

See project documentation for licensing information.
