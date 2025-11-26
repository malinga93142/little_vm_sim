/*
 * Author: RK
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define RAM_SIZE        (1 << 20) // 1 MB
#define PAGE_SIZE       4096
#define NUM_PHYS_PAGES  (RAM_SIZE/PAGE_SIZE)


#define L1_ENTRIES  16
#define L2_ENTRIES  16

#define PTE_VALID 0x01
#define PTE_WRITE 0x02
#define PTE_READ  0x04


typedef struct{
  int phys_page;
  uint8_t flags;  // valid, read/write permissions
} L2Entry;


typedef struct{
  L2Entry entries[L2_ENTRIES];
} L2Table;

typedef struct{
  L2Table *tables[L1_ENTRIES];
} L1Table;

typedef struct{
  uint32_t page_faults;
  uint32_t reads;
  uint32_t writes;
  uint32_t translation_failures;
} VMStats;



uint8_t RAM[RAM_SIZE];
L1Table page_table;
bool phys_pages_used[NUM_PHYS_PAGES];
VMStats stats;

int page_fault_handler(uint16_t virt_page);
int allocate_phys_page(void);


void init_vm(void){
  memset(&page_table, 0, sizeof(page_table));
  memset(phys_pages_used, 0, sizeof(phys_pages_used));
  memset(&stats, 0, sizeof(stats));
}




L2Table* allocate_L2(){
  L2Table *t = malloc(sizeof(L2Table));
  if (!t){
    fprintf(stderr, "ERROR: mem alloc failed\n");
    return NULL;
  }
  for (int i = 0; i < L2_ENTRIES; i++){
    t->entries[i].phys_page = -1;
    t->entries[i].flags     = 0;
  }
  return t;
}

int allocate_phys_page(void){
  for (int i = 0; i < NUM_PHYS_PAGES; i++){
    if (!phys_pages_used[i]){
      phys_pages_used[i] = true;
      memset(&RAM[i*PAGE_SIZE], 0, PAGE_SIZE);
      return i;
    }
  }
  return -1;
}


void free_phys_page(int phys_page){
  if (phys_page >= 0 && phys_page < NUM_PHYS_PAGES){
    phys_pages_used[phys_page] = false;
  }
}



int map_page(uint16_t virt_page, uint16_t phys_page, uint8_t flags){
  if (virt_page >= (L1_ENTRIES * L2_ENTRIES)){
    fprintf(stderr, "ERROR: virt page 0x%x oob\n", virt_page);
    return -1;
  }


  if (phys_page >= NUM_PHYS_PAGES){
    fprintf(stderr, "ERROR: phys page %d oob\n", phys_page);
    return -1;
  }
  uint8_t l1 = (virt_page >> 4) & 0xf;
  uint8_t l2 = virt_page & 0xf;

  if (page_table.tables[l1] == NULL){
    page_table.tables[l1] = allocate_L2();
    if (!page_table.tables[l1])
      return -1;
  }

  page_table.tables[l1]->entries[l2].phys_page 	= phys_page;
  page_table.tables[l1]->entries[l2].flags			=	flags | PTE_VALID;
	phys_pages_used[phys_page] = true;
  return 0;
}

int translate(uint32_t vaddr, uint32_t *out_paddr, bool is_write){
  if (vaddr >= RAM_SIZE){
		fprintf(stderr, "ERROR: virt address 0x%x exceeds addr space\n", vaddr);
		stats.translation_failures++;
		return -1;
	}
	uint8_t l1 = (vaddr >> 16) & 0xf;
  uint8_t l2 = (vaddr >> 12) & 0xf;
  uint16_t off=(vaddr & 0xfff);
  L2Table *t = page_table.tables[l1];
  if (!t){
		stats.translation_failures++;
		return -1;
	}

  L2Entry *entry 	= &t->entries[l2];
	int phys_page 	=	entry->phys_page;
  if (phys_page < 0 || !(entry->flags & PTE_VALID)){
		stats.translation_failures++;
		return -1;
	}

	if (is_write && !(entry->flags & PTE_WRITE)){
		fprintf(stderr, "ERROR: write perm denied at addr 0x%x\n", vaddr);
		stats.translation_failures++;
		return -2;
	}
	if (!is_write && !(entry->flags & PTE_READ)){
		stats.translation_failures++;
		return -2;
	}
	uint32_t paddr = phys_page * PAGE_SIZE + off;
	if (paddr >= RAM_SIZE){
		fprintf(stderr, "ERROR: phys addr 0x%x oob\n", paddr);
		stats.translation_failures++;
		return -1;
	}
  *out_paddr = paddr;
  return 0;
}

int unmap_page(uint16_t virt_page){
	if (virt_page >= (L1_ENTRIES * L2_ENTRIES)){
		return -1;
	}

	uint8_t l1 = (virt_page >> 4) & 0xf;
	uint8_t l2 = virt_page & 0xf;
	L2Table *t = page_table.tables[l1];
	if (!t)	return -1;

	int phys_page = t->entries[l2].phys_page;
	if (phys_page >= 0){
		free_phys_page(phys_page);
	}
	t->entries[l2].phys_page = -1;
	t->entries[l2].flags = 0;
	return 0;
}

int page_fault_handler(uint16_t virt_page){
	stats.page_faults++;
	printf("page fault: virt page 0x%x\n", virt_page);

	int phys_page = allocate_phys_page();
	if (phys_page < 0){
		fprintf(stderr, "ERROR: oopm\n");
		return -1;
	}
	printf("	-> allocated physical page %d\n", phys_page);
	return map_page(virt_page, phys_page, PTE_READ | PTE_WRITE);
}
int write_vmem(uint32_t vaddr, uint8_t val){
  uint32_t paddr;
  int res = translate(vaddr, &paddr, true);
	if (res == -1){
		uint16_t virt_page = vaddr >> 12;
		if (page_fault_handler(virt_page) != 0)	return -1;
		res = translate(vaddr, &paddr, true);
	}
	if (res != 0)
		return -1;
	RAM[paddr] = val;
	stats.writes++;
	return 0;
}

int read_vmem(uint32_t vaddr, uint8_t *out){
  uint32_t paddr;
  int res = translate(vaddr, &paddr, false);
	if (res == -1){
		uint16_t virt_page = vaddr >> 12;
		if (page_fault_handler(virt_page) != 0)	return -1;
		res = translate(vaddr, &paddr, false);
	}
	if (res != 0)	return -1;
  *out = RAM[paddr];
	stats.reads++;
  return 0;
}

void
free_pages(void){
  for (int i = 0; i < L1_ENTRIES; i++){
    if (page_table.tables[i] == NULL)  continue;
    free(page_table.tables[i]);
		page_table.tables[i] = NULL;
  }
}


void
print_stats(void){
	printf("\n=== Virt Mem Stats ===\n");
	printf("%-12s:  %u\n", "Page faults", stats.page_faults);
	printf("%-12s:  %u\n", "Reads", stats.reads);
	printf("%-12s:  %u\n", "Writes",stats.writes);
	printf("%-12s:  %u\n", "Trans fails",stats.translation_failures);
	int used_pages= 0;
	for (int i = 0; i < NUM_PHYS_PAGES; i++){
		if (phys_pages_used[i])	used_pages++;
	}
printf("%-12s:  %d / %d\n", "PHY used",  used_pages, NUM_PHYS_PAGES);
}
/*int main(){
  uint8_t RO = PTE_READ;
  uint8_t WO = PTE_WRITE;
  uint8_t RW = RO | WO;
  init_vm();
	map_page(0x00, 42, RW);
  write_vmem(0x000510, 0xab);
  uint8_t x;
  read_vmem(0x000510, &x);
  printf("0x%2x\n", x);
  print_stats();
  write_vmem(0x0100ab, 0xde);
  read_vmem(0x0100ab, &x);
  printf("0x%2X\n", x);
  print_stats();
  free_pages();
  return 0;
}*/
