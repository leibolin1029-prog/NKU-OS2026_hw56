#include <pmm.h>
#include <stdio.h>
#include <buddy_pmm.h>
#include <buddy_test.h>

#ifdef BUDDY_TEST

/* 测试开始时的空闲页数，用于每次测试后的状态复原检查 */
static size_t initial_free;

static void test_init_state(void) {
    cprintf("buddy_test: init state\n");
    assert(nr_free_pages() > 0);
    assert(nr_free_pages() == initial_free);
}

static void test_single_page(void) {
    cprintf("buddy_test: single page\n");
    size_t before = nr_free_pages();

    struct Page *p = alloc_page();
    assert(p != NULL);
    assert(!PageProperty(p));
    assert(page2pa(p) >= (nbase << PGSHIFT));
    assert(PPN(page2pa(p)) < npage);
    assert(nr_free_pages() == before - 1);

    free_page(p);
    assert(nr_free_pages() == before);
}

static void test_power_of_two(void) {
    cprintf("buddy_test: power of two\n");
    size_t sizes[4] = {1, 2, 4, 8};

    for (int i = 0; i < 4; i++) {
        size_t n = sizes[i];
        size_t before = nr_free_pages();

        struct Page *p = alloc_pages(n);
        assert(p != NULL);

        size_t idx = PPN(page2pa(p)) - nbase;
        assert((idx % n) == 0);
        for (size_t k = 1; k < n; k++) {
            assert(!PageProperty(p + k));
        }
        assert(nr_free_pages() == before - n);

        free_pages(p, n);
        assert(nr_free_pages() == before);
    }
}

static void test_non_power_of_two(void) {
    cprintf("buddy_test: non power of two\n");
    size_t requests[2] = {3, 5};
    size_t actual[2] = {4, 8};

    for (int i = 0; i < 2; i++) {
        size_t before = nr_free_pages();

        struct Page *p = alloc_pages(requests[i]);
        assert(p != NULL);
        assert(nr_free_pages() == before - actual[i]);

        free_pages(p, requests[i]);
        assert(nr_free_pages() == before);
    }
}

static void test_split_and_merge(void) {
    cprintf("buddy_test: split and merge\n");
    size_t before = nr_free_pages();

    struct Page *big = alloc_pages(8);
    assert(big != NULL);
    assert(nr_free_pages() == before - 8);

    free_pages(big, 8);
    assert(nr_free_pages() == before);

    struct Page *small = alloc_page();
    assert(small != NULL);
    assert(nr_free_pages() == before - 1);

    free_page(small);
    assert(nr_free_pages() == before);
}

static void test_fragmentation(void) {
    cprintf("buddy_test: fragmentation\n");
    size_t before = nr_free_pages();

    struct Page *blk = alloc_pages(8);
    assert(blk != NULL);
    assert(nr_free_pages() == before - 8);

    /* 把 8 页块切成 4 页 + 2 页，制造碎片 */
    free_pages(blk, 4);
    free_pages(blk + 4, 2);
    assert(nr_free_pages() == before - 2);

    /* 通过块头的阶数确认碎片未被合并：4 页块为 order 2，2 页块为 order 1 */
    assert(PageProperty(blk) && blk->property == 2);
    assert(PageProperty(blk + 4) && blk[4].property == 1);

    /* 归还最后 2 页，三个碎片应逐级合并回一个 8 页块（order 3） */
    free_pages(blk + 6, 2);
    assert(nr_free_pages() == before);
    assert(PageProperty(blk) && blk->property == 3);

    /* 合并后该 8 页块可再次整体分配 */
    struct Page *again = alloc_pages(8);
    assert(again != NULL);
    free_pages(again, 8);
    assert(nr_free_pages() == before);
}

static struct Page *cached_pages[512];

static void test_many_allocations(void) {
    cprintf("buddy_test: many allocations\n");
    size_t before = nr_free_pages();
    size_t count = 0;

    while (count < 512) {
        struct Page *p = alloc_page();
        if (p == NULL) {
            break;
        }
        cached_pages[count++] = p;
    }
    assert(count > 0);
    assert(nr_free_pages() == before - count);

    while (count > 0) {
        count--;
        free_page(cached_pages[count]);
    }
    assert(nr_free_pages() == before);
}

static void test_boundary(void) {
    cprintf("buddy_test: boundary\n");
    size_t before = nr_free_pages();

    /* 申请超过空闲总量的页数必须失败，且不改变任何计数 */
    assert(alloc_pages(before + 1) == NULL);
    assert(nr_free_pages() == before);

    /* 申请最大阶块：可用内存不足时应失败，若成功则立即归还 */
    size_t max_block = (size_t)1 << (BUDDY_MAX_ORDER - 1);
    struct Page *huge = alloc_pages(max_block);
    if (huge != NULL) {
        free_pages(huge, max_block);
    }
    assert(nr_free_pages() == before);
}

void buddy_test(void) {
    cprintf("buddy_test: start\n");
    initial_free = nr_free_pages();
    assert(initial_free > 0);

    test_init_state();
    test_single_page();
    test_power_of_two();
    test_non_power_of_two();
    test_split_and_merge();
    test_fragmentation();
    test_many_allocations();
    test_boundary();

    assert(nr_free_pages() == initial_free);
    cprintf("buddy_test: ALL PASSED\n");
}

#endif /* BUDDY_TEST */
