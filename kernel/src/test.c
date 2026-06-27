/*
 * CoolPotOS 单元测试
 * 测试 VFS 和任务调度器基本功能
 * 编译: 此文件用于验证内核子系统接口的正确性
 *
 * 注意: 这些测试在 kernel 模式下运行，不使用标准库
 */

#include "fs/vfs.h"
#include "task/task.h"
#include "krlibc.h"
#include "term/kprint.h"
#include "mem/slub.h"
#include "klog.h"
#include "kconfig.h"
#include "ksecure.h"

/* ---- 测试框架 ---- */
static int  g_tests_passed = 0;
static int  g_tests_failed = 0;
static char g_test_name[128];

#define TEST(name)                                    \
    do {                                              \
        strncpy(g_test_name, name, sizeof(g_test_name) - 1); \
        g_test_name[sizeof(g_test_name) - 1] = '\0';  \
    } while (0)

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            kerror("  FAIL: %s (%s:%d)", g_test_name, __FILE__, __LINE__); \
            g_tests_failed++;                                             \
            return;                                                       \
        }                                                                 \
    } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_NE(a, b) CHECK((a) != (b))
#define CHECK_NOT_NULL(p) CHECK((p) != NULL)
#define CHECK_NULL(p) CHECK((p) == NULL)

#define TEST_PASSED()                                               \
    do {                                                            \
        ksuccess("  PASS: %s", g_test_name);                       \
        g_tests_passed++;                                           \
    } while (0)

/* ---- VFS 测试 ---- */

static void test_vfs_init(void) {
    TEST("VFS initialization");
    vfs_init();
    vfs_inode_t *root = vfs_get_root_inode();
    CHECK_NOT_NULL(root);
    CHECK_EQ(root->type, VFS_TYPE_DIR);
    TEST_PASSED();
}

static void test_vfs_create_file(void) {
    TEST("VFS create file");
    int ret = vfs_create("/test.txt", VFS_TYPE_FILE);
    CHECK_EQ(ret, 0);

    vfs_dentry_t *dentry = vfs_lookup("/test.txt");
    CHECK_NOT_NULL(dentry);
    CHECK_NOT_NULL(dentry->inode);
    CHECK_EQ(dentry->inode->type, VFS_TYPE_FILE);
    TEST_PASSED();
}

static void test_vfs_create_dir(void) {
    TEST("VFS create directory");
    int ret = vfs_mkdir("/testdir");
    CHECK_EQ(ret, 0);

    vfs_dentry_t *dentry = vfs_lookup("/testdir");
    CHECK_NOT_NULL(dentry);
    CHECK_NOT_NULL(dentry->inode);
    CHECK_EQ(dentry->inode->type, VFS_TYPE_DIR);
    TEST_PASSED();
}

static void test_vfs_open_read_write(void) {
    TEST("VFS open/read/write/close");
    int fd = vfs_open("/test_rw.txt", O_CREAT | O_RDWR, 0644);
    CHECK(fd >= 0);

    const char *msg = "Hello, CoolPotOS!";
    ssize_t written = vfs_write(fd, msg, strlen(msg));
    CHECK_EQ(written, (ssize_t)strlen(msg));

    /* 回到文件开头重新读取 */
    int fd2 = vfs_open("/test_rw.txt", O_RDONLY, 0);
    CHECK(fd2 >= 0);

    char buf[64] = {0};
    ssize_t read_bytes = vfs_read(fd2, buf, sizeof(buf) - 1);
    CHECK_EQ(read_bytes, (ssize_t)strlen(msg));
    CHECK_EQ(strcmp(buf, msg), 0);

    vfs_close(fd);
    vfs_close(fd2);
    TEST_PASSED();
}

static void test_vfs_unlink(void) {
    TEST("VFS unlink file");
    int ret = vfs_create("/to_delete.txt", VFS_TYPE_FILE);
    CHECK_EQ(ret, 0);

    ret = vfs_unlink("/to_delete.txt");
    CHECK_EQ(ret, 0);

    vfs_dentry_t *dentry = vfs_lookup("/to_delete.txt");
    CHECK_NULL(dentry);
    TEST_PASSED();
}

static void test_vfs_lookup_root(void) {
    TEST("VFS lookup root");
    vfs_dentry_t *dentry = vfs_lookup("/");
    CHECK_NOT_NULL(dentry);
    CHECK_NOT_NULL(dentry->inode);
    CHECK_EQ(dentry->inode->type, VFS_TYPE_DIR);
    TEST_PASSED();
}

/* ---- 任务调度器测试 ---- */

static volatile int test_task_counter = 0;

static void test_task_entry(void) {
    test_task_counter++;
    task_exit(0);
}

static void test_task_create_basic(void) {
    TEST("Task create basic");
    task_t *task = task_create("test_task", test_task_entry, TASK_DEFAULT_PRIORITY, TASK_TYPE_KERNEL);
    CHECK_NOT_NULL(task);
    CHECK_EQ(task->state, TASK_READY);
    CHECK_EQ(task->priority, TASK_DEFAULT_PRIORITY);
    CHECK_EQ(strcmp(task->name, "test_task"), 0);
    TEST_PASSED();
}

static void test_task_find_by_pid(void) {
    TEST("Task find by PID");
    task_t *task = task_create("findme", test_task_entry, 64, TASK_TYPE_KERNEL);
    CHECK_NOT_NULL(task);

    task_t *found = task_find_by_pid(task->pid);
    CHECK_NOT_NULL(found);
    CHECK_EQ(found->pid, task->pid);
    TEST_PASSED();
}

static void test_task_get_current(void) {
    TEST("Task get current");
    task_t *current = task_get_current();
    /* 在初始化后，当前任务应该是 idle */
    CHECK_NOT_NULL(current);
    TEST_PASSED();
}

static void test_task_get_pid(void) {
    TEST("Task get PID");
    int pid = task_get_pid();
    CHECK(pid >= 0);
    TEST_PASSED();
}

static void test_task_sleep_wake(void) {
    TEST("Task sleep/wake");
    task_t *task = task_create("sleeper", test_task_entry, 64, TASK_TYPE_KERNEL);
    CHECK_NOT_NULL(task);

    task->state = TASK_SLEEPING;
    task->sleep_until = 100;

    CHECK_EQ(task->state, TASK_SLEEPING);
    task_wakeup(task);
    CHECK_EQ(task->state, TASK_READY);
    TEST_PASSED();
}

static void test_task_block_unblock(void) {
    TEST("Task block/unblock");
    task_t *task = task_create("blocker", test_task_entry, 64, TASK_TYPE_KERNEL);
    CHECK_NOT_NULL(task);

    task_block(task);
    CHECK_EQ(task->state, TASK_BLOCKED);

    task_unblock(task);
    CHECK_EQ(task->state, TASK_READY);
    TEST_PASSED();
}

/* ---- 内存分配器测试 ---- */

static void test_malloc_free(void) {
    TEST("malloc/free basic");
    void *ptr = malloc(128);
    CHECK_NOT_NULL(ptr);
    memset(ptr, 0xAB, 128);
    free(ptr);
    TEST_PASSED();
}

static void test_calloc(void) {
    TEST("calloc zeros memory");
    int *ptr = calloc(10, sizeof(int));
    CHECK_NOT_NULL(ptr);
    for (int i = 0; i < 10; i++) {
        CHECK_EQ(ptr[i], 0);
    }
    free(ptr);
    TEST_PASSED();
}

static void test_realloc(void) {
    TEST("realloc grow");
    char *ptr = malloc(64);
    CHECK_NOT_NULL(ptr);
    memset(ptr, 'A', 64);

    char *new_ptr = realloc(ptr, 128);
    CHECK_NOT_NULL(new_ptr);
    CHECK_EQ(new_ptr[0], 'A');
    CHECK_EQ(new_ptr[63], 'A');
    free(new_ptr);
    TEST_PASSED();
}

static void test_aligned_alloc(void) {
    TEST("aligned_alloc");
    void *ptr = aligned_alloc(64, 256);
    CHECK_NOT_NULL(ptr);
    CHECK_EQ((uintptr_t)ptr % 64, 0);
    free(ptr);
    TEST_PASSED();
}

/* ---- 字符串测试 ---- */

static void test_strcmp(void) {
    TEST("strcmp equality");
    CHECK_EQ(strcmp("hello", "hello"), 0);
    CHECK_NE(strcmp("hello", "world"), 0);
    CHECK(strcmp("a", "b") < 0);
    TEST_PASSED();
}

static void test_strlen(void) {
    TEST("strlen");
    CHECK_EQ(strlen(""), 0);
    CHECK_EQ(strlen("hello"), 5);
    CHECK_EQ(strlen("CoolPotOS"), 9);
    TEST_PASSED();
}

static void test_strcpy(void) {
    TEST("strcpy");
    char buf[32];
    char *ret = strcpy(buf, "test");
    CHECK_EQ(strcmp(buf, "test"), 0);
    CHECK_EQ(ret, buf);  /* 返回 dest */
    TEST_PASSED();
}

static void test_memset_memcpy(void) {
    TEST("memset/memcpy");
    char src[16] = "source";
    char dst[16];
    memset(dst, 0, sizeof(dst));
    memcpy(dst, src, strlen(src) + 1);
    CHECK_EQ(strcmp(dst, "source"), 0);
    TEST_PASSED();
}

/* ---- 位图测试 ---- */

static void test_bitmap(void) {
    TEST("bitmap basic operations");
    uint8_t buf[16];
    Bitmap bm;
    bitmap_init(&bm, buf, sizeof(buf));

    /* 初始状态全为 0 */
    CHECK_EQ(bitmap_get(&bm, 0), false);
    CHECK_EQ(bitmap_get(&bm, 63), false);

    /* 设置和读取 */
    bitmap_set(&bm, 10, true);
    CHECK_EQ(bitmap_get(&bm, 10), true);
    CHECK_EQ(bitmap_get(&bm, 11), false);

    bitmap_set(&bm, 10, false);
    CHECK_EQ(bitmap_get(&bm, 10), false);

    /* 范围设置 */
    bitmap_set_range(&bm, 0, 32, true);
    CHECK_EQ(bitmap_get(&bm, 0), true);
    CHECK_EQ(bitmap_get(&bm, 31), true);
    CHECK_EQ(bitmap_get(&bm, 32), false);

    /* 范围查找 */
    size_t idx = bitmap_find_range(&bm, 8, true);
    CHECK_EQ(idx, 0);  /* 前32位都是true */

    idx = bitmap_find_range(&bm, 8, false);
    CHECK_EQ(idx, 32); /* 第32位开始是false */

    TEST_PASSED();
}

/* ---- 日志系统测试 ---- */

static void test_klog_write(void) {
    TEST("Klog basic write");
    klog_write(KLOG_INFO, "TEST", "test message %d", 42);
    size_t count = klog_get_count();
    CHECK(count > 0);
    const klog_entry_t *entry = klog_get_entry(count - 1);
    CHECK_NOT_NULL(entry);
    CHECK_EQ(entry->level, KLOG_INFO);
    TEST_PASSED();
}

static void test_klog_level_filter(void) {
    TEST("Klog level filter");
    klog_set_level(KLOG_WARN);
    klog_write(KLOG_DEBUG, "TEST", "should be filtered");
    klog_write(KLOG_INFO, "TEST", "should be filtered");
    klog_write(KLOG_WARN, "TEST", "should appear");
    /* 恢复默认级别 */
    klog_set_level(KLOG_INFO);
    TEST_PASSED();
}

static void test_klog_module_tag(void) {
    TEST("Klog module tag");
    klog_write(KLOG_INFO, "VFS", "test message");
    size_t count = klog_get_count();
    const klog_entry_t *entry = klog_get_entry(count - 1);
    CHECK_NOT_NULL(entry);
    CHECK_EQ(strcmp(entry->tag, "VFS"), 0);
    TEST_PASSED();
}

/* ---- 配置系统测试 ---- */

static void test_kconfig_basic(void) {
    TEST("Kconfig set/get");
    int ret = kconfig_set("kernel.loglevel", "2");
    CHECK_EQ(ret, 0);
    const char *val = kconfig_get("kernel.loglevel");
    CHECK_NOT_NULL(val);
    CHECK_EQ(strcmp(val, "2"), 0);
    TEST_PASSED();
}

static void test_kconfig_types(void) {
    TEST("Kconfig typed get");
    /* 使用已注册的配置项 */
    kconfig_set("kernel.loglevel", "3");
    CHECK_EQ(kconfig_get_int("kernel.loglevel"), 3);

    kconfig_set("debug.stack_canary", "1");
    CHECK_EQ(kconfig_get_bool("debug.stack_canary"), true);

    kconfig_set("debug.stack_canary", "0");
    CHECK_EQ(kconfig_get_bool("debug.stack_canary"), false);

    /* 恢复默认值 */
    kconfig_reset_defaults();
    TEST_PASSED();
}

/* ---- 安全模块测试 ---- */

static void test_ksecure_stack_canary(void) {
    TEST("Ksecure stack canary");
    uint64_t canary = ksecure_get_canary();
    CHECK(canary != 0);
    /* LSB 应为 0 (防止字符串泄露) */
    CHECK_EQ(canary & 0xFF, 0);
    CHECK(ksecure_check_canary(canary));
    CHECK(!ksecure_check_canary(0xDEADBEEF));
    TEST_PASSED();
}

static void test_ksecure_ptr_check(void) {
    TEST("Ksecure pointer validation");
    /* 内核指针检查 */
    CHECK_EQ(ksecure_is_kernel_ptr((void *)0xFFFF800000000000ULL), true);
    CHECK_EQ(ksecure_is_kernel_ptr((void *)0x1000), false);
    CHECK_EQ(ksecure_is_kernel_ptr(NULL), false);

    /* 用户指针检查 */
    CHECK_EQ(ksecure_is_user_ptr((void *)0x1000, 100), true);
    CHECK_EQ(ksecure_is_user_ptr((void *)0xFFFF800000000000ULL, 100), false);
    TEST_PASSED();
}

static void test_ksecure_memzero(void) {
    TEST("Ksecure memzero");
    char buf[16];
    memset(buf, 'A', sizeof(buf));
    ksecure_memzero(buf, sizeof(buf));
    for (int i = 0; i < 16; i++) {
        CHECK_EQ(buf[i], 0);
    }
    TEST_PASSED();
}

static void test_ksecure_strncpy(void) {
    TEST("Ksecure safe strncpy");
    char dst[8];
    size_t n = ksecure_strncpy(dst, "hello", sizeof(dst));
    CHECK_EQ(n, 5);
    CHECK_EQ(strcmp(dst, "hello"), 0);

    /* 截断测试 */
    n = ksecure_strncpy(dst, "very long string", sizeof(dst));
    CHECK_EQ(n, 7);
    CHECK_EQ(dst[7], '\0');
    TEST_PASSED();
}

static void test_ksecure_aslr(void) {
    TEST("Ksecure ASLR");
    uint64_t offset = ksecure_aslr_offset();
    /* ASLR 偏移应为页对齐 */
    CHECK_EQ(offset & 0xFFF, 0);
    TEST_PASSED();
}

/* ---- 集成测试 ---- */

static void test_integration_vfs_task(void) {
    TEST("Integration: VFS + Task");
    /* 创建文件 */
    int fd = vfs_open("/integ_test.txt", O_CREAT | O_RDWR, 0644);
    CHECK(fd >= 0);

    const char *msg = "Integration test data";
    ssize_t written = vfs_write(fd, msg, strlen(msg));
    CHECK_EQ(written, (ssize_t)strlen(msg));

    /* 创建任务验证调度器运行 */
    task_t *task = task_create("integ_test", test_task_entry, 64, TASK_TYPE_KERNEL);
    CHECK_NOT_NULL(task);

    vfs_close(fd);
    vfs_unlink("/integ_test.txt");
    TEST_PASSED();
}

static void test_integration_klog_config(void) {
    TEST("Integration: Klog + Kconfig");
    /* 通过配置设置日志级别 */
    kconfig_set("kernel.loglevel", "1");
    int64_t level = kconfig_get_int("kernel.loglevel");
    klog_set_level((klog_level_t)level);

    klog_write(KLOG_WARN, "INTEG", "warning should appear");
    size_t count_before = klog_get_count();

    klog_write(KLOG_DEBUG, "INTEG", "debug should be filtered");
    size_t count_after = klog_get_count();

    /* 只增加了1条日志（WARN），DEBUG被过滤 */
    CHECK_EQ(count_after - count_before, 1);

    /* 恢复默认 */
    klog_set_level(KLOG_INFO);
    TEST_PASSED();
}

/* ---- 运行所有测试 ---- */

void run_all_tests(void) {
    printk("\n========== CoolPotOS Unit Tests ==========\n\n");

    /* VFS 测试 */
    test_vfs_init();
    test_vfs_lookup_root();
    test_vfs_create_file();
    test_vfs_create_dir();
    test_vfs_open_read_write();
    test_vfs_unlink();

    /* 任务调度器测试 */
    test_task_create_basic();
    test_task_find_by_pid();
    test_task_get_current();
    test_task_get_pid();
    test_task_sleep_wake();
    test_task_block_unblock();

    /* 内存分配器测试 */
    test_malloc_free();
    test_calloc();
    test_realloc();
    test_aligned_alloc();

    /* 字符串测试 */
    test_strcmp();
    test_strlen();
    test_strcpy();
    test_memset_memcpy();

    /* 位图测试 */
    test_bitmap();

    /* 日志系统测试 */
    test_klog_write();
    test_klog_level_filter();
    test_klog_module_tag();

    /* 配置系统测试 */
    test_kconfig_basic();
    test_kconfig_types();

    /* 安全模块测试 */
    test_ksecure_stack_canary();
    test_ksecure_ptr_check();
    test_ksecure_memzero();
    test_ksecure_strncpy();
    test_ksecure_aslr();

    /* 集成测试 */
    test_integration_vfs_task();
    test_integration_klog_config();

    /* 总结 */
    printk("\n========== Test Results ==========\n");
    ksuccess("Passed: %d", g_tests_passed);
    if (g_tests_failed > 0) {
        kerror("Failed: %d", g_tests_failed);
    } else {
        ksuccess("Failed: %d", g_tests_failed);
    }
    printk("Total: %d\n", g_tests_passed + g_tests_failed);
    printk("==================================\n");
}