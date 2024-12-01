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

static void morecore(int bucket);

static int findbucket(union overhead *freep, int srchlen);

static void *sbrk(int incr){
    pcb_t *pcb = get_current_proc();
    if (pcb->program_break + incr >= pcb->program_break_end) {
        ral:
        if(pcb->program_break_end >= (void*)0x01bf8f7d) goto alloc_error; // 奇怪的界限, 不设置内核会卡死
        if ((uint32_t) pcb->program_break_end < USER_AREA_START) {
            uint32_t ai = (uint32_t) pcb->program_break_end;
            for (; ai < (uint32_t) pcb->program_break_end + PAGE_SIZE * 10;) {
                alloc_frame(get_page(ai, 1, pcb->pgd_dir), 1, 0);
                ai += PAGE_SIZE;
            }
            pcb->program_break_end = (void *) ai;

            if (pcb->program_break + incr >= pcb->program_break_end) {
                goto ral;
            }
        } else {
            alloc_error:
            klogf(false, "OUT_OF_MEMORY_ERROR: Cannot alloc user heap.\n");
            printk("UserHeapEnd: %08x\n", pcb->program_break_end);
            return (void *) -1;
        }
    }

    void *prev_break = pcb->program_break;
    pcb->program_break += incr;
    return prev_break;
}

void *user_malloc(size_t nbytes) {
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
    return ((char *) (op + 1));
}

void *user_calloc(size_t nelem, size_t elsize) {
    void *ptr = user_malloc(nelem * elsize);
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

void user_free(void *cp) {
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
}


/* EDB: added size lookup */

size_t user_usable_size(void *cp) {
    register union overhead *op;

    if (cp == NULL)
        return 0;
    op = (union overhead *) ((caddr_t) cp - sizeof(union overhead));

    if (op->ov_magic != MAGIC)
        return 0;                /* sanity */

    return op->ov_index;
}

static int realloc_srchlen = 4;    /* 4 should be plenty, -1 =>'s whole list */

void *user_realloc(void *cp, size_t nbytes) {
    register u_long onb;
    register long i;
    union overhead *op;
    char *res;
    int was_alloced = 0;

    if (cp == NULL)
        return (user_malloc(nbytes));
    if (nbytes == 0) {
        user_free(cp);
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
            user_free(cp);
    }
    if ((res = user_malloc(nbytes)) == NULL)
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