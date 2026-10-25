#ifndef __KERN_MM_BUDDY_TEST_H__
#define __KERN_MM_BUDDY_TEST_H__

/* Buddy System 分配器的自测入口；仅在编译时定义 BUDDY_TEST 时生效 */
void buddy_test(void);

#endif /* !__KERN_MM_BUDDY_TEST_H__ */
