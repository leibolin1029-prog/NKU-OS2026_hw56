#include <pmm.h>
#include <list.h>
#include <stdio.h>
#include <buddy_pmm.h>

/* 每个阶维护一条空闲链表，链表节点为块头 Page 的 page_link 字段；
 * 块头的 PG_property 置位，Page.property 记录该块的阶数。 */
static list_entry_t free_lists[BUDDY_MAX_ORDER];

/* 所有阶上空闲块包含的页数总和 */
static unsigned int nr_free;

/* 返回不小于 n 的最小 2 的幂的阶数 */
static size_t order_of_pages(size_t n) {
    size_t order = 0;
    while (((size_t)1 << order) < n) {
        order++;
    }
    return order;
}

static void buddy_init(void) {
    for (int i = 0; i < BUDDY_MAX_ORDER; i++) {
        list_init(&free_lists[i]);
    }
    nr_free = 0;
}

/* 把 [base, base+n) 切成若干 2^order 对齐的块并挂入对应阶的链表 */
static void buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }
    nr_free += (unsigned int)n;

    size_t idx = PPN(page2pa(base)) - nbase;
    while (n > 0) {
        size_t order = 0;
        while ((order + 1) < BUDDY_MAX_ORDER &&
               ((size_t)1 << (order + 1)) <= n &&
               (idx % ((size_t)1 << (order + 1))) == 0) {
            order++;
        }
        struct Page *head = pages + idx;
        head->property = (unsigned int)order;
        SetPageProperty(head);
        list_add(&free_lists[order], &head->page_link);

        idx += (size_t)1 << order;
        n -= (size_t)1 << order;
    }
}

static struct Page *buddy_alloc_pages(size_t n) {
    assert(n > 0);
    size_t order = order_of_pages(n);
    if (order >= BUDDY_MAX_ORDER || n > nr_free) {
        return NULL;
    }

    /* 找到最小的、仍有空闲块的阶 */
    size_t cur = order;
    while (cur < BUDDY_MAX_ORDER && list_empty(&free_lists[cur])) {
        cur++;
    }
    if (cur >= BUDDY_MAX_ORDER) {
        return NULL;
    }

    list_entry_t *le = list_next(&free_lists[cur]);
    struct Page *block = le2page(le, page_link);
    list_del(&block->page_link);

    /* 逐级分裂：每一级把右半块挂到低一阶的链表上 */
    while (cur > order) {
        cur--;
        struct Page *buddy = block + ((size_t)1 << cur);
        buddy->property = (unsigned int)cur;
        SetPageProperty(buddy);
        list_add(&free_lists[cur], &buddy->page_link);
    }

    block->property = (unsigned int)order;
    ClearPageProperty(block);
    nr_free -= (unsigned int)((size_t)1 << order);
    return block;
}

static void buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    size_t order = order_of_pages(n);
    if (order >= BUDDY_MAX_ORDER) {
        return;
    }

    base->property = (unsigned int)order;
    SetPageProperty(base);
    nr_free += (unsigned int)((size_t)1 << order);

    size_t idx = PPN(page2pa(base)) - nbase;

    /* 伙伴合并：伙伴页号为 idx XOR 2^order，同阶且空闲才可合并 */
    while ((order + 1) < BUDDY_MAX_ORDER) {
        size_t buddy_idx = idx ^ ((size_t)1 << order);
        if (buddy_idx >= npage - nbase) {
            break;
        }
        struct Page *buddy = pages + buddy_idx;
        if (!PageProperty(buddy) || buddy->property != (unsigned int)order) {
            break;
        }

        list_del(&buddy->page_link);
        ClearPageProperty(buddy);
        buddy->property = 0;
        ClearPageProperty(base);

        size_t low = buddy_idx < idx ? buddy_idx : idx;
        base = pages + low;
        idx = low;
        order++;
        base->property = (unsigned int)order;
        SetPageProperty(base);
    }

    list_add(&free_lists[order], &base->page_link);
}

static size_t buddy_nr_free_pages(void) {
    return (size_t)nr_free;
}

/* 自检：块头标志、阶数、对齐、总数守恒，以及块之间不重叠 */
static void buddy_check(void) {
    size_t starts[BUDDY_MAX_ORDER * 8];
    size_t lens[BUDDY_MAX_ORDER * 8];
    int cnt = 0;
    size_t total = 0;

    for (int order = 0; order < BUDDY_MAX_ORDER; order++) {
        list_entry_t *le = &free_lists[order];
        while ((le = list_next(le)) != &free_lists[order]) {
            struct Page *p = le2page(le, page_link);
            assert(PageProperty(p));
            assert(p->property == (unsigned int)order);

            size_t idx = PPN(page2pa(p)) - nbase;
            assert(idx < npage - nbase);
            assert((idx % ((size_t)1 << order)) == 0);

            total += (size_t)1 << order;
            if (cnt < (int)(sizeof(starts) / sizeof(starts[0]))) {
                starts[cnt] = idx;
                lens[cnt] = (size_t)1 << order;
                cnt++;
            }
        }
    }

    assert(total == (size_t)nr_free);

    for (int i = 0; i < cnt; i++) {
        for (int j = i + 1; j < cnt; j++) {
            assert(starts[i] + lens[i] <= starts[j] ||
                   starts[j] + lens[j] <= starts[i]);
        }
    }

    cprintf("buddy_check() succeeded!\n");
}

const struct pmm_manager buddy_pmm_manager = {
    .name          = "buddy_pmm_manager",
    .init          = buddy_init,
    .init_memmap   = buddy_init_memmap,
    .alloc_pages   = buddy_alloc_pages,
    .free_pages    = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check         = buddy_check,
};
