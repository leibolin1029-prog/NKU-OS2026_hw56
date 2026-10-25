#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

/* 伙伴系统支持的最大阶数：order ∈ [0, BUDDY_MAX_ORDER-1]，
 * 最大块为 2^(BUDDY_MAX_ORDER-1) 页。 */
#define BUDDY_MAX_ORDER 16

extern const struct pmm_manager buddy_pmm_manager;

#endif /* !__KERN_MM_BUDDY_PMM_H__ */
