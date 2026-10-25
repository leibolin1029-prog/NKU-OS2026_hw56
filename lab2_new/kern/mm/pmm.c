#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <buddy_pmm.h>
#include <buddy_test.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
#ifdef BUDDY_TEST
    pmm_manager = &buddy_pmm_manager;   /* Challenge 自测时启用伙伴系统 */
#else
    pmm_manager = &best_fit_pmm_manager;
#endif
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    return pmm_manager->alloc_pages(n);
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    pmm_manager->free_pages(base, n);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    return pmm_manager->nr_free_pages();
}

//lab2 function1 2412362
/* page_init - 根据设备树探测到的物理内存范围建立 Page 描述符数组：
 * 先把 [nbase, npage) 中的物理页全部保留，再把内核镜像与描述符数组之后、
 * 内存末端之前的完整页区间交给当前物理内存管理器。 */
static void page_init(void) {
    // 内核运行在虚拟地址空间，物理地址 = 虚拟地址 - 虚实映射偏移
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    // 物理内存范围由 dtb_init() 解析设备树得到
    uint64_t mem_base = get_memory_base();
    uint64_t mem_size = get_memory_size();
    if (mem_base == 0 || mem_size == 0) {
        panic("page_init: invalid physical memory range from DTB");
    }

    // npage 为可管理物理页号的上界，nbase 为 DRAM 起始物理页号，
    // 因此描述符数组需要覆盖 [nbase, npage) 中的每个物理页
    npage = PPN(mem_base + mem_size);
    if (npage <= nbase) {
        panic("page_init: npage (%lu) is not larger than nbase (%lu)",
              (unsigned long)npage, (unsigned long)nbase);
    }

    // 描述符数组紧随内核镜像之后，并按页对齐（end 为内核镜像结尾的虚拟地址）
    extern char end[];
    pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE);

    // 布局完成之前先把所有页置为保留，避免未初始化的页被提前分配；
    // 同时建立物理页号与描述符数组下标的对应关系：下标 = PPN - nbase
    for (size_t i = nbase; i < npage; i++) {
        struct Page *p = &pages[i - nbase];
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
        SetPageReserved(p);
    }

    cprintf("physical memory map:\n");
    cprintf("  memory: 0x%016lx, [0x%016lx, 0x%016lx].\n",
            mem_size, mem_base, mem_base + mem_size - 1);

    // 描述符数组之后的地址向上按页对齐，内存末端向下按页对齐，
    // 只有两者之间的完整页才可以交给分配器
    uintptr_t free_start_pa =
        PADDR(ROUNDUP((uintptr_t)(pages + (npage - nbase)), PGSIZE));
    uintptr_t free_end_pa = ROUNDDOWN(mem_base + mem_size, PGSIZE);

    // 没有可用完整页面时不注册空闲区
    if (free_start_pa < free_end_pa) {
        init_memmap(pa2page(free_start_pa), (free_end_pa - free_start_pa) / PGSIZE);
    } else {
        cprintf("page_init: no complete free page after the kernel\n");
    }
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);

#ifdef BUDDY_TEST
    buddy_test();
#endif
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
