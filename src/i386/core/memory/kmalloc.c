#include "kmalloc.h"
#include "klog.h"
#include "krlibc.h"
#include "page.h"

extern page_directory_t *kernel_directory; // page.c

void *program_break = (void *)0x3e0000;
void *program_break_end;

#define getpagesize() PAGE_SIZE

uint32_t kh_usage_memory_byte;

static void *sbrk(int incr) { // 内核堆扩容措施
    if (program_break == 0) { return (void *)-1; }

    if (program_break + incr >= program_break_end) {
    ral:
        if (program_break_end >= (void *)0x01bf8f7d)
            goto alloc_error; // 奇怪的界限, 不设置内核会卡死
        if ((uint32_t)program_break_end < USER_AREA_START) {
            uint32_t ai = (uint32_t)program_break_end;
            for (; ai < (uint32_t)program_break_end + PAGE_SIZE * 10;) {
                alloc_frame(get_page(ai, 1, kernel_directory), 1, 0);
                ai += PAGE_SIZE;
            }
            program_break_end = (void *)ai; // 重新设置堆末尾指针

            if (program_break + incr >= program_break_end) { // 处理物理内存不足的错误
                goto ral;
            }
        } else {
        alloc_error:
            klogf(false, "OUT_OF_MEMORY_ERROR: Cannot alloc kernel heap.\n");
            printk("KernelHeapEnd: %08x\n", program_break_end);
            while (1)
                __asm__("hlt");
            return (void *)-1;
        }
    }

    void *prev_break  = program_break;
    program_break    += incr;
    return prev_break;
}

union overhead {
    union overhead *ov_next; /* when free */
    struct {
        u_char ovu_magic; /* magic number */
        u_char ovu_index; /* bucket # */
#ifdef RCHECK
        u_short ovu_rmagic; /* range magic number */
        u_long  ovu_size;   /* actual block size */
#endif
    } ovu;
#define ov_magic  ovu.ovu_magic
#define ov_index  ovu.ovu_index
#define ov_rmagic ovu.ovu_rmagic
#define ov_size   ovu.ovu_size
};

#define MAGIC  0xef   /* magic # on accounting info */
#define RMAGIC 0x5555 /* magic # on range info */

#define RSLOP 0

#define NBUCKETS 30
static union overhead *nextf[NBUCKETS];

static int pagesz;     /* page size */
static int pagebucket; /* page size bucket */

#define ASSERT(p)

static void morecore(int bucket);

static int findbucket(union overhead *freep, int srchlen);

extern uint32_t kh_usage_memory_byte; // free_page.c

void *kmalloc(size_t nbytes) {
    register union overhead *op;
    register int             bucket;
    register long            n;
    register unsigned        amt;

    if (pagesz == 0) { // 初始化操作
        pagesz = n = getpagesize();
        op         = (union overhead *)sbrk(0);
        n          = n - sizeof(*op) - ((long)op & (n - 1)); // 计算可用内存
        if (n < 0) n += pagesz;
        if (n) {
            if (sbrk(n) == (char *)-1) { return (NULL); }
        }
        bucket = 0;
        amt    = 8;
        while (pagesz > amt) { // 寻找合适的桶索引
            amt <<= 1;
            bucket++;
        }
        pagebucket = bucket;
    }
    if (nbytes <= (n = pagesz - sizeof(*op) - RSLOP)) { // 判断占用内存块大小是否小于页面大小
        amt    = 8;                                     /* size of first bucket */
        bucket = 0;
        n      = -((long)sizeof(*op) + RSLOP); // 使用较小的内存块
    } else {
        amt    = pagesz;
        bucket = pagebucket;
    }
    while (nbytes > amt + n) { // 找到合适的内存块大小
        amt <<= 1;
        if (amt == 0) {
            return (NULL); // 处理内存块溢出
        }
        bucket++;
    }
    if ((op = nextf[bucket]) == NULL) {
        morecore(bucket);                   // 调用函数扩展内存
        if ((op = nextf[bucket]) == NULL) { // 扩展失败，返回NULL
            return (NULL);
        }
    }
    // 从链表中移除内存块
    nextf[bucket]         = op->ov_next;
    op->ov_magic          = MAGIC;
    op->ov_index          = bucket;
    kh_usage_memory_byte += bucket;
    return ((char *)(op + 1));
}

void *kcalloc(size_t nelem, size_t elsize) {
    void *ptr = kmalloc(nelem * elsize); // 调用函数分配内存
    memset(ptr, 0, nelem * elsize);      // 格式化内存（这么说应该比较合适来着QwQ）
    return ptr;                          // 返回内存指针
}

static void morecore(int bucket) {
    register union overhead *op;
    register long            sz;    // 所需内存块大小
    long                     amt;   // 分配的总内存大小
    int                      nblks; // 实际分配的内存块数量

    sz = 1 << (bucket + 3); // 计算内存块大小
    if (sz <= 0)            // 内存块大小不足，分配失败
        return;
    if (sz < pagesz) { // 若所需内存块小于页面大小，分配一个页面大小的内存
        amt   = pagesz;
        nblks = amt / sz;
    } else { // 否则分配所需内存块+一个页面大小的内存
        amt   = sz + pagesz;
        nblks = 1;
    }
    op = (union overhead *)sbrk(amt); // 分配内存
    if ((long)op == -1) return;
    nextf[bucket] = op; // 初始化内存块链表
    while (--nblks > 0) {
        op->ov_next = (union overhead *)((caddr_t)op + sz);
        op          = (union overhead *)((caddr_t)op + sz);
    }
}

void kfree(void *cp) { // 在内核中释放内存
    register long            size;
    register union overhead *op;

    if (cp == NULL) // 检查空指针
        return;
    op = (union overhead *)((caddr_t)cp - sizeof(union overhead)); // 计算内存块开销
#ifdef DEBUG
    ASSERT(op->ov_magic == MAGIC); // 确保内存块在使用中
#else
    if (op->ov_magic != MAGIC) return; /* sanity */
#endif
#ifdef RCHECK
    ASSERT(op->ov_rmagic == RMAGIC);                                 // 确保右边界有效
    ASSERT(*(u_short *)((caddr_t)(op + 1) + op->ov_size) == RMAGIC); // 确保内存块末尾有效
#endif
    size = op->ov_index;       // 计算内存块大小
    ASSERT(size < NBUCKETS);   // 验证桶索引
    op->ov_next = nextf[size]; // 将内存块插入链表
    nextf[size] = op;
#ifdef MSTATS // 更新内存统计信息
    nmalloc[size]--;
#endif
    kh_usage_memory_byte -= size;
}

/* EDB: added size lookup */

size_t kmalloc_usable_size(void *cp) {
    register union overhead *op;

    if (cp == NULL) return 0;
    op = (union overhead *)((caddr_t)cp - sizeof(union overhead)); // 计算内存块的开销信息

    if (op->ov_magic != MAGIC) // 验证内存是否有效
        return 0;              /* sanity */

    return op->ov_index;
}

static int realloc_srchlen = 4; /* 4 should be plenty, -1 =>'s whole list */

void *krealloc(void *cp, size_t nbytes) { // 用于在内核中重新分配内存
    register u_long onb;
    register long   i;
    union overhead *op;
    char           *res;
    int             was_alloced = 0;

    if (cp == NULL) // 处理特殊情况
        return (kmalloc(nbytes));
    if (nbytes == 0) {
        kfree(cp);
        return NULL;
    }
    op = (union overhead *)((caddr_t)cp - sizeof(union overhead)); // 获取开销信息
    if (op->ov_magic == MAGIC) {                                   // 检查内存块是否有效
        was_alloced++;                                             // 标记内存块为已分配
        i = op->ov_index;                                          // 获取桶索引
    } else {
        if ((i = findbucket(op, 1)) < 0 && // 尝试查找内存块
            (i = findbucket(op, realloc_srchlen)) < 0)
            i = NBUCKETS;
    }
    onb = 1 << (i + 3); // 计算当前内存块大小
    if (onb < pagesz)
        onb -= sizeof(*op) + RSLOP;
    else
        onb += pagesz - sizeof(*op) - RSLOP;

    if (was_alloced) { // 避免不必要的复制
        if (i) {
            i = 1 << (i + 2);
            if (i < pagesz)
                i -= sizeof(*op) + RSLOP;
            else
                i += pagesz - sizeof(*op) - RSLOP;
        }
        if (nbytes <= onb && nbytes > i) {
            return (cp);
        } else
            kfree(cp);
    }
    if ((res = kmalloc(nbytes)) == NULL) // 分配新的内存块
        return (NULL);
    if (cp != res)                                     /* common optimization if "compacting" */
        bcopy(cp, res, (nbytes < onb) ? nbytes : onb); // 复制数据
    return (res);
}

static int findbucket(union overhead *freep, int srchlen) {
    register union overhead *p;
    register int             i, j;

    for (i = 0; i < NBUCKETS; i++) { // 遍历所有桶
        j = 0;
        for (p = nextf[i]; p && j != srchlen; p = p->ov_next) { // 遍历当前桶的内存块链表
            if (p == freep)                                     // 找到合适的内存块，返回索引
                return (i);
            j++;
        }
    }
    return (-1); // 查找失败
}
