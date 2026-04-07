#ifndef MEMORY_H
#define MEMORY_H

/* types */
typedef unsigned long  u64;
typedef unsigned int   u32;
typedef unsigned char  u8;

/* QEMU virt machine RAM */
#define RAM_START    0x40000000UL
#define RAM_SIZE     (128UL * 1024UL * 1024UL)
#define RAM_END      (RAM_START + RAM_SIZE)

/* page size = 4KB (same as OS page!) */
#define PAGE_SIZE    4096UL
#define PAGE_SHIFT   12
#define PAGE_MASK    (~(PAGE_SIZE - 1))

/* round address UP to page boundary */
#define PAGE_ALIGN(x)  (((x) + PAGE_SIZE - 1) & PAGE_MASK)

/* convert between addresses and page numbers */
#define PHYS_TO_PAGE(addr)  ((addr) >> PAGE_SHIFT)
#define PAGE_TO_PHYS(page)  ((page) << PAGE_SHIFT)

/* physical memory manager */
void  pmm_init(u64 start, u64 end);
u64   pmm_alloc_page(void);
void  pmm_free_page(u64 addr);
u64   pmm_free_count(void);

/* simple heap allocator */
void  heap_init(void);
void* kmalloc(u64 size);

/* page table */
void  mmu_init(void);

#endif
