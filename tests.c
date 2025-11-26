#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "pages.c"
// Test helper macros
#define TEST_START(name) printf("\n=== TEST: %s ===\n", name)
#define TEST_PASS(msg) printf("  ✓ PASS: %s\n", msg)
#define TEST_FAIL(msg) printf("  ✗ FAIL: %s\n", msg)
#define ASSERT(cond, msg) if (cond) TEST_PASS(msg); else TEST_FAIL(msg)

void test_basic_read_write(void) {
    TEST_START("Basic Read/Write");
    init_vm();
    
    map_page(0x00, 10, PTE_READ | PTE_WRITE);
    
    // Write and read back
    int result = write_vmem(0x000100, 0xAA);
    ASSERT(result == 0, "Write to mapped page");
    
    uint8_t val;
    result = read_vmem(0x000100, &val);
    ASSERT(result == 0 && val == 0xAA, "Read returns correct value");
    
    free_pages();
}

void test_page_fault_handling(void) {
    TEST_START("Page Fault Handling");
    init_vm();
    
    uint32_t old_faults = stats.page_faults;
    
    // Write to unmapped page should trigger page fault
    int result = write_vmem(0x005000, 0xBB);
    ASSERT(result == 0, "Page fault handled successfully");
    ASSERT(stats.page_faults == old_faults + 1, "Page fault counter incremented");
    
    // Second access should not fault
    old_faults = stats.page_faults;
    uint8_t val;
    result = read_vmem(0x005000, &val);
    ASSERT(result == 0 && val == 0xBB, "No fault on second access");
    ASSERT(stats.page_faults == old_faults, "No additional page fault");
    
    free_pages();
}

void test_permissions(void) {
    TEST_START("Permission Checking");
    init_vm();
    
    // Read-only page
    map_page(0x01, 20, PTE_READ);
    RAM[20 * PAGE_SIZE + 50] = 0xCC;
    
    uint8_t val;
    int result = read_vmem(0x001032, &val);
    ASSERT(result == 0, "Read from read-only page succeeds");
    
    result = write_vmem(0x001032, 0xDD);
    ASSERT(result != 0, "Write to read-only page fails");
    
    // Write-only page (unusual but valid)
    map_page(0x02, 21, PTE_WRITE);
    result = write_vmem(0x002100, 0xEE);
    ASSERT(result == 0, "Write to write-only page succeeds");
    
    result = read_vmem(0x002100, &val);
    ASSERT(result != 0, "Read from write-only page fails");
    
    // Read-write page
    map_page(0x03, 22, PTE_READ | PTE_WRITE);
    result = write_vmem(0x003200, 0xFF);
    ASSERT(result == 0, "Write to RW page succeeds");
    
    result = read_vmem(0x003200, &val);
    ASSERT(result == 0 && val == 0xFF, "Read from RW page succeeds");
    
    free_pages();
}

void test_bounds_checking(void) {
    TEST_START("Bounds Checking");
    init_vm();
    
    // Invalid virtual address (beyond 1MB)
    int result = write_vmem(0x200000, 0x11);
    ASSERT(result != 0, "Reject address beyond virtual space");
    
    // Invalid physical page in manual mapping
    result = map_page(0x04, 999, PTE_READ | PTE_WRITE);
    ASSERT(result != 0, "Reject invalid physical page");
    
    // Invalid virtual page
    result = map_page(0xFFF, 30, PTE_READ | PTE_WRITE);
    ASSERT(result != 0, "Reject invalid virtual page");
    
    free_pages();
}

void test_multiple_pages(void) {
    TEST_START("Multiple Page Operations");
    init_vm();
    
    // Write to different pages
    for (int i = 0; i < 10; i++) {
        uint32_t addr = i * PAGE_SIZE;
        write_vmem(addr, i);
    }
    
    // Verify all pages
    bool all_correct = true;
    for (int i = 0; i < 10; i++) {
        uint8_t val;
        uint32_t addr = i * PAGE_SIZE;
        if (read_vmem(addr, &val) != 0 || val != i) {
            all_correct = false;
            break;
        }
    }
    ASSERT(all_correct, "All 10 pages read/write correctly");
    
    free_pages();
}

void test_page_boundaries(void) {
    TEST_START("Page Boundary Handling");
    init_vm();
    
    map_page(0x05, 30, PTE_READ | PTE_WRITE);
    
    // Write at start of page
    write_vmem(0x005000, 0x11);
    uint8_t val;
    read_vmem(0x005000, &val);
    ASSERT(val == 0x11, "Access at page start");
    
    // Write at end of page
    write_vmem(0x005FFF, 0x22);
    read_vmem(0x005FFF, &val);
    ASSERT(val == 0x22, "Access at page end");
    
    // Access just beyond page boundary (different page)
    int result = write_vmem(0x006000, 0x33);
    ASSERT(result != 0, "Access to unmapped adjacent page fails");
    
    free_pages();
}

void test_unmap_page(void) {
    TEST_START("Page Unmapping");
    init_vm();
    
    map_page(0x06, 40, PTE_READ | PTE_WRITE);
    write_vmem(0x006100, 0x44);
    
    uint8_t val;
    int result = read_vmem(0x006100, &val);
    ASSERT(result == 0, "Read from mapped page");
    
    unmap_page(0x06);
    result = read_vmem(0x006100, &val);
    ASSERT(result != 0, "Read from unmapped page fails");
    
    free_pages();
}

void test_large_data_transfer(void) {
    TEST_START("Large Data Transfer");
    init_vm();
    
    // Write a pattern across multiple pages
    for (uint32_t addr = 0; addr < 0x10000; addr += 256) {
        write_vmem(addr, (addr / 256) & 0xFF);
    }
    
    // Verify the pattern
    bool pattern_correct = true;
    for (uint32_t addr = 0; addr < 0x10000; addr += 256) {
        uint8_t val;
        if (read_vmem(addr, &val) != 0 || val != ((addr / 256) & 0xFF)) {
            pattern_correct = false;
            break;
        }
    }
    ASSERT(pattern_correct, "Large sequential data transfer correct");
    
    free_pages();
}

void test_physical_memory_exhaustion(void) {
    TEST_START("Physical Memory Exhaustion");
    init_vm();
    
    // Try to allocate all physical pages
    int allocated = 0;
    for (int i = 0; i < NUM_PHYS_PAGES + 10; i++) {
        uint32_t addr = i * PAGE_SIZE;
        if (write_vmem(addr, i & 0xFF) == 0) {
            allocated++;
        } else {
            break;
        }
    }
    
    ASSERT(allocated == NUM_PHYS_PAGES, "Allocated all available physical pages");
    
    // Next allocation should fail
    int result = write_vmem((NUM_PHYS_PAGES + 1) * PAGE_SIZE, 0x99);
    ASSERT(result != 0, "Allocation fails when memory exhausted");
    
    free_pages();
}

void test_two_level_table_structure(void) {
    TEST_START("Two-Level Table Structure");
    init_vm();
    
    // Map pages in different L1 entries
    map_page(0x00, 50, PTE_READ | PTE_WRITE);  // L1=0, L2=0
    map_page(0x1F, 51, PTE_READ | PTE_WRITE);  // L1=1, L2=15
    map_page(0xAB, 52, PTE_READ | PTE_WRITE);  // L1=10, L2=11
    
    write_vmem(0x000100, 0xAA);
    write_vmem(0x01F200, 0xBB);
    write_vmem(0x0AB300, 0xCC);
    
    uint8_t v1, v2, v3;
    read_vmem(0x000100, &v1);
    read_vmem(0x01F200, &v2);
    read_vmem(0x0AB300, &v3);
    
    ASSERT(v1 == 0xAA && v2 == 0xBB && v3 == 0xCC, 
           "Pages in different L1/L2 entries work correctly");
    
    free_pages();
}

void run_all_tests(void) {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  Virtual Memory Test Suite             ║\n");
    printf("╚════════════════════════════════════════╝\n");
    
    test_basic_read_write();
    test_page_fault_handling();
    test_permissions();
    test_bounds_checking();
    test_multiple_pages();
    test_page_boundaries();
    test_unmap_page();
    test_large_data_transfer();
    test_physical_memory_exhaustion();
    test_two_level_table_structure();
    
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  All Tests Completed                   ║\n");
    printf("╚════════════════════════════════════════╝\n");
}

int main() {
    run_all_tests();
    return 0;
}
