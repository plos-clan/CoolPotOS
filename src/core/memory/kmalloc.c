#include "kmalloc.h"
#include "krlibc.h"
#include "page.h"
#include "klog.h"

extern page_directory_t *kernel_directory; //page.c

void *program_break = (void *) 0x3e0000;
void *program_break_end;

#define getpagesize() PAGE_SIZE

uint32_t kh_usage_memory_byte;

static void *sbrk(int incr) { //内核堆扩容措施
    if (program_break == 0) {
        return (void *) -1;
    }

    if (program_break + incr >= program_break_end) {
        ral:
        if (program_break_end >= (void *) 0x01bf8f7d) goto alloc_error; // 奇怪的界限, 不设置内核会卡死
        if ((uint32_t) program_break_end < USER_AREA_START) {
            uint32_t ai = (uint32_t) program_break_end;
            for (; ai < (uint32_t) program_break_end + PAGE_SIZE * 10;) {
                alloc_frame(get_page(ai, 1, kernel_directory), 1, 0);
                ai += PAGE_SIZE;
            }
            program_break_end = (void *) ai;

            if (program_break + incr >= program_break_end) {
                goto ral;
            }
        } else {
            alloc_error:
            klogf(false, "OUT_OF_MEMORY_ERROR: Cannot alloc kernel heap.\n");
            printk("KernelHeapEnd: %08x\n", program_break_end);
            while (1) __asm__("hlt");
            return (void *) -1;
        }
    }

    void *prev_break = program_break;
    program_break += incr;
    return prev_break;
}

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

static void morecore(int bucket);

static int findbucket(union overhead *freep, int srchlen);

extern uint32_t kh_usage_memory_byte; //free_page.c

void *kmalloc(size_t nbytes) {
    register union overhead *op;
    register int bucket;
    register long n;
    register unsigned amt;

    if (pagesz == 0) {
        pagesz = n = getpagesize();
        op = (union overhead *) sbrk(0);
        n = n - sizeof(*op) - ((long) op & (n - 1));
        if (n < 0)
            n += pagesz;
        if (n) {
            if (sbrk(n) == (char *) -1) {
                return (NULL);
            }
        }
        bucket = 0;
        amt = 8;
        while (pagesz > amt) {
            amt <<= 1;
            bucket++;
        }
        pagebucket = bucket;
    }
    if (nbytes <= (n = pagesz - sizeof(*op) - RSLOP)) {
        amt = 8;    /* size of first bucket */
        bucket = 0;
        n = -((long) sizeof(*op) + RSLOP);
    } else {
        amt = pagesz;
        bucket = pagebucket;
    }
    while (nbytes > amt + n) {
        amt <<= 1;
        if (amt == 0) {
            return (NULL);
        }
        bucket++;
    }
    if ((op = nextf[bucket]) == NULL) {
        morecore(bucket);
        if ((op = nextf[bucket]) == NULL) {
            return (NULL);
        }
    }
    /* remove from linked list */
    nextf[bucket] = op->ov_next;
    op->ov_magic = MAGIC;
    op->ov_index = bucket;
    kh_usage_memory_byte += bucket;
    return ((char *) (op + 1));
}

void *kcalloc(size_t nelem, size_t elsize) {
    void *ptr = kmalloc(nelem * elsize);
    memset(ptr, 0, nelem * elsize);
    return ptr;
}

static void morecore(int bucket) {
    register union overhead *op;
    register long sz;        /* size of desired block */
    long amt;            /* amount to allocate */
    int nblks;            /* how many blocks we get */

    sz = 1 << (bucket + 3);
    if (sz <= 0)
        return;
    if (sz < pagesz) {
        amt = pagesz;
        nblks = amt / sz;
    } else {
        amt = sz + pagesz;
        nblks = 1;
    }
    op = (union overhead *) sbrk(amt);
    if ((long) op == -1)
        return;
    nextf[bucket] = op;
    while (--nblks > 0) {
        op->ov_next = (union overhead *) ((caddr_t) op + sz);
        op = (union overhead *) ((caddr_t) op + sz);
    }
}

void kfree(void *cp) {
    register long size;
    register union overhead *op;

    if (cp == NULL)
        return;
    op = (union overhead *) ((caddr_t) cp - sizeof(union overhead));
#ifdef DEBUG
    ASSERT(op->ov_magic == MAGIC);		/* make sure it was in use */
#else
    if (op->ov_magic != MAGIC)
        return;                /* sanity */
#endif
#ifdef RCHECK
    ASSERT(op->ov_rmagic == RMAGIC);
    ASSERT(*(u_short *)((caddr_t)(op + 1) + op->ov_size) == RMAGIC);
#endif
    size = op->ov_index;
    ASSERT(size < NBUCKETS);
    op->ov_next = nextf[size];    /* also clobbers ov_magic */
    nextf[size] = op;
#ifdef MSTATS
    nmalloc[size]--;
#endif
    kh_usage_memory_byte -= size;
}


/* EDB: added size lookup */

size_t kmalloc_usable_size(void *cp) {
    register union overhead *op;

    if (cp == NULL)
        return 0;
    op = (union overhead *) ((caddr_t) cp - sizeof(union overhead));

    if (op->ov_magic != MAGIC)
        return 0;                /* sanity */

    return op->ov_index;
}

static int realloc_srchlen = 4;    /* 4 should be plenty, -1 =>'s whole list */

void *krealloc(void *cp, size_t nbytes) {
    register u_long onb;
    register long i;
    union overhead *op;
    char *res;
    int was_alloced = 0;

    if (cp == NULL)
        return (kmalloc(nbytes));
    if (nbytes == 0) {
        kfree(cp);
        return NULL;
    }
    op = (union overhead *) ((caddr_t) cp - sizeof(union overhead));
    if (op->ov_magic == MAGIC) {
        was_alloced++;
        i = op->ov_index;
    } else {
        if ((i = findbucket(op, 1)) < 0 &&
            (i = findbucket(op, realloc_srchlen)) < 0)
            i = NBUCKETS;
    }
    onb = 1 << (i + 3);
    if (onb < pagesz)
        onb -= sizeof(*op) + RSLOP;
    else
        onb += pagesz - sizeof(*op) - RSLOP;
    /* avoid the copy if same size block */
    if (was_alloced) {
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
    if ((res = kmalloc(nbytes)) == NULL)
        return (NULL);
    if (cp != res)        /* common optimization if "compacting" */
        bcopy(cp, res, (nbytes < onb) ? nbytes : onb);
    return (res);
}

static int findbucket(union overhead *freep, int srchlen) {
    register union overhead *p;
    register int i, j;

    for (i = 0; i < NBUCKETS; i++) {
        j = 0;
        for (p = nextf[i]; p && j != srchlen; p = p->ov_next) {
            if (p == freep)
                return (i);
            j++;
        }
    }
    return (-1);
}
