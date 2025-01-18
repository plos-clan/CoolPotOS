#include "user_malloc.h"
#include "krlibc.h"
#include "scheduler.h"
#include "klog.h"

#define u_char unsigned char
#define u_long unsigned long
#define getpagesize() 4096
#define caddr_t size_t
#define bcopy(a, b, c) memcpy(b,a,c)

union overhead {
    union overhead *ov_next;    /* when free */
    struct {
        u_char ovu_magic;    /* magic number */
        u_char ovu_index;    /* bucket # */
#ifdef RCHECK
        u_short	ovu_rmagic;	/* range magic number */
        u_long	ovu_size;	/* actual block size */
#endif
    } ovu;
#define    ov_magic    ovu.ovu_magic
#define    ov_index    ovu.ovu_index
#define    ov_rmagic    ovu.ovu_rmagic
#define    ov_size        ovu.ovu_size
};

#define    MAGIC        0xef        /* magic # on accounting info */
#define RMAGIC        0x5555        /* magic # on range info */

#define    RSLOP        0

#define    NBUCKETS 30
static union overhead *nextf[NBUCKETS];

static int pagesz;            /* page size */
static int pagebucket;            /* page size bucket */

#define    ASSERT(p)

static void morecore(pcb_t *pcb, int bucket);

static int findbucket(union overhead *freep, int srchlen);

static void *sbrk(pcb_t *pcb, int incr) {
    //pcb_t *pcb = get_current_proc();
    if (pcb->program_break + incr >= pcb->program_break_end) {//检查堆扩展是否超限
        ral:
        if ((uint32_t) pcb->program_break_end < USER_AREA_START) {//判断是否存在可扩展空间
            uint32_t ai = (uint32_t) pcb->program_break_end;
            for (; ai < (uint32_t) pcb->program_break_end + PAGE_SIZE * 10;) {//循环分配10个页面大小的内存
                alloc_frame(get_page(ai, 1, pcb->pgd_dir), 1, 0);
                ai += PAGE_SIZE;//增加一个页面大小
            }
            pcb->program_break_end = (void *) ai;//更新堆的末尾地址

            if (pcb->program_break + incr >= pcb->program_break_end) {//若堆扩展超过堆末尾
                goto ral;
            }
        } else {
            alloc_error:
            klogf(false, "OUT_OF_MEMORY_ERROR: Cannot alloc user heap.\n");//记录错误信息
            printk("UserHeapEnd: %08x\n", pcb->program_break_end);//记录当前堆末尾地址
            return (void *) -1;
        }
    }

    void *prev_break = pcb->program_break;//保存当前堆顶地址
    pcb->program_break += incr;//更新堆顶地址
    return prev_break;//返回堆顶地址
}

void *user_malloc(pcb_t *pcb, size_t nbytes) {
    register union overhead *op;
    register int bucket;
    register long n;
    register unsigned amt;

    if (pagesz == 0) {//初始化页面大小
        pagesz = n = getpagesize();//获取系统页面大小
        op = (union overhead *) sbrk(pcb, 0);//获取当前的堆尾地址
        n = n - sizeof(*op) - ((long) op & (n - 1));//计算可用内存
        if (n < 0)
            n += pagesz;//若剩余内存不足，扩展一个页面
        if (n) {//若剩余内存不为0，尝试扩展堆
            if (sbrk(pcb, n) == (char *) -1) {
                return (NULL);
            }
        }
        bucket = 0;//初始化桶索引和内存块大小
        amt = 8;
        while (pagesz > amt) {//寻找合适的桶索引
            amt <<= 1;
            bucket++;
        }
        pagebucket = bucket;
    }
    if (nbytes <= (n = pagesz - sizeof(*op) - RSLOP)) {//若请求内存较小，使用较小的内存块
        amt = 8;    //初始化内存块
        bucket = 0;
        n = -((long) sizeof(*op) + RSLOP);
    } else { //否则使用页面大小的内存块
        amt = pagesz;
        bucket = pagebucket;
    }
    while (nbytes > amt + n) {//查找合适大小的内存块
        amt <<= 1;//超级加倍（bushi
        if (amt == 0) {//判断是否溢出
            return (NULL);
        }
        bucket++;//增加桶索引
    }
    if ((op = nextf[bucket]) == NULL) {
        morecore(pcb, bucket);//若当前桶索引对应的内存块链表为空，调用函数扩展内存
        if ((op = nextf[bucket]) == NULL) {//判断扩展是否成功
            return (NULL);
        }
    }

    nextf[bucket] = op->ov_next;//从链表中移除内存块
    op->ov_magic = MAGIC;//设置魔法数
    op->ov_index = bucket;//设置桶索引
    return ((char *) (op + 1));//内存块的起始地址
}

void *user_calloc(size_t nelem, size_t elsize) {
    void *ptr = user_malloc(get_current_proc(), nelem * elsize);//分配内存
    memset(ptr, 0, nelem * elsize);//初始化内存
    return ptr;//返回内存指针
}

static void morecore(pcb_t *pcb, int bucket) {
    register union overhead *op;
    register long sz;        //所需内存块的大小
    long amt;               //要分配的总内存大小
    int nblks;             //实际分配的内存块数量

    sz = 1 << (bucket + 3);//计算内存块大小
    if (sz <= 0)//分配失败
        return;
    if (sz < pagesz) {
        amt = pagesz;//如果所需内存块大小小于页面大小，分配一个页面大小的内存
        nblks = amt / sz;//计算可分配的内存块数量
    } else {
        amt = sz + pagesz;//否则分配所需内存块大小加上一个页面大小的内存
        nblks = 1;//只允许分配一个内存块
    }
    op = (union overhead *) sbrk(pcb, amt);//分配内存
    if ((long) op == -1)//内存分配失败，直接返回
        return;
    nextf[bucket] = op;//初始化内存块链表
    while (--nblks > 0) {
        op->ov_next = (union overhead *) ((caddr_t) op + sz);//设置下一个内存块指针
        op = (union overhead *) ((caddr_t) op + sz);//移动指针至下一个内存块的首地址
    }
}

void user_free(void *cp) {
    register long size;
    register union overhead *op;

    if (cp == NULL)//检查空指针
        return;
    op = (union overhead *) ((caddr_t) cp - sizeof(union overhead));
#ifdef DEBUG
    ASSERT(op->ov_magic == MAGIC);//确保内存块在使用中
#else
    if (op->ov_magic != MAGIC)
        return;                //直接返回，不进行释放操作
#endif
#ifdef RCHECK//若宏被定义，进行额外验证
    ASSERT(op->ov_rmagic == RMAGIC);//确保右边界有效
    ASSERT(*(u_short *)((caddr_t)(op + 1) + op->ov_size) == RMAGIC);//确保内存块的末尾有效
#endif
    size = op->ov_index;//计算内存块大小
    ASSERT(size < NBUCKETS);//验证桶索引
    op->ov_next = nextf[size];//将内存块插入链表
    nextf[size] = op;
#ifdef MSTATS
    nmalloc[size]--;//更新内存统计信息
#endif
}


/* EDB: added size lookup */

size_t user_usable_size(void *cp) {
    register union overhead *op;

    if (cp == NULL)//检查空指针
        return 0;
    op = (union overhead *) ((caddr_t) cp - sizeof(union overhead));//计算内存块开销

    if (op->ov_magic != MAGIC)//验证内存有效性
        return 0;

    return op->ov_index;//返回实际可用大小
}

static int realloc_srchlen = 4;    /* 4 should be plenty, -1 =>'s whole list */

void *user_realloc(pcb_t *pcb, void *cp, size_t nbytes) {
    register u_long onb;
    register long i;
    union overhead *op;
    char *res;
    int was_alloced = 0;

    if (cp == NULL)//判断空指针
        return (user_malloc(pcb, nbytes));
    if (nbytes == 0) {//判断新内存块大小是否为0
        user_free(cp);
        return NULL;
    }
    op = (union overhead *) ((caddr_t) cp - sizeof(union overhead));//计算内存块开销信息
    if (op->ov_magic == MAGIC) {//验证内存有效性
        was_alloced++;
        i = op->ov_index;
    } else {
        if ((i = findbucket(op, 1)) < 0 &&
            (i = findbucket(op, realloc_srchlen)) < 0)
            i = NBUCKETS;
    }
    onb = 1 << (i + 3);//计算当前内存块大小
    if (onb < pagesz)
        onb -= sizeof(*op) + RSLOP;
    else
        onb += pagesz - sizeof(*op) - RSLOP;

    if (was_alloced) {//避免不必要的复制
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
            user_free(cp);
    }
    if ((res = user_malloc(pcb, nbytes)) == NULL)//分配新的内存块
        return (NULL);
    if (cp != res)//复制数据
        bcopy(cp, res, (nbytes < onb) ? nbytes : onb);
    return (res);
}

static int findbucket(union overhead *freep, int srchlen) {
    register union overhead *p;
    register int i, j;

    for (i = 0; i < NBUCKETS; i++) {//遍历所有桶
        j = 0;
        for (p = nextf[i]; p && j != srchlen; p = p->ov_next) {//遍历当前桶的内存块链表
            if (p == freep)//查找当前内存块的桶索引
                return (i);
            j++;
        }
    }
    return (-1);
}