#include "mem/slub.h"
#include "arch.h"
#include "krlibc.h"
#include "lock.h"
#include "mem/frame.h"
#include "mem/page.h"

#define SLUB_ALIGN        16UL
#define SLUB_MAGIC        0x534c5542U
#define SLUB_FLAG_LARGE   0x0001U
#define SLUB_FLAG_ALIGNED 0x0002U
#define SLUB_CACHE_COUNT  11

typedef struct slub_cache slub_cache_t;
typedef struct slub_slab slub_slab_t;

typedef struct slub_free_node {
    struct slub_free_node *next;
} slub_free_node_t;

typedef struct slub_alloc_header {
    uint32_t magic;
    uint16_t cache_index;
    uint16_t flags;
    size_t requested_size;
    union {
        slub_slab_t *slab;
        size_t pages;
        void *raw_ptr;
    };
    uintptr_t caller;
    size_t alignment;
} slub_alloc_header_t;

#define SLUB_HEADER_SIZE ((sizeof(slub_alloc_header_t) + SLUB_ALIGN - 1) & ~(SLUB_ALIGN - 1))

struct slub_slab {
    slub_slab_t *next;
    slub_slab_t *prev;
    slub_cache_t *cache;
    slub_free_node_t *free_list;
    uintptr_t phys;
    uint16_t total_objects;
    uint16_t free_objects;
    bool on_partial;
};

struct slub_cache {
    size_t object_size;
    size_t slab_pages;
    slub_slab_t *partial;
    spin_t lock;
};

static const size_t slub_class_sizes[SLUB_CACHE_COUNT] = { 64,  96,  128,  192,  256, 384,
                                                           512, 768, 1024, 1536, 2048 };

static slub_cache_t slub_caches[SLUB_CACHE_COUNT];
static spin_t slub_init_lock  = SPIN_INIT;
static bool slub_initialized  = false;
static bool slub_initializing = false;

static size_t slub_align_up(size_t value, size_t align) {
    return (value + align - 1) & ~(align - 1);
}

static uintptr_t slub_align_up_ptr(uintptr_t value, size_t align) {
    return (value + align - 1) & ~(uintptr_t)(align - 1);
}

static bool slub_is_power_of_two(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

void slub_init(void) {
    bool irq = arch_check_interrupt();
    arch_close_interrupt();

    spin_lock(slub_init_lock);
    if (slub_initialized || slub_initializing) {
        spin_unlock(slub_init_lock);
        if (irq)
            arch_open_interrupt();
        return;
    }

    slub_initializing = true;

    for (size_t i = 0; i < SLUB_CACHE_COUNT; i++) {
        slub_caches[i].object_size = slub_class_sizes[i];
        slub_caches[i].slab_pages  = 1;
        slub_caches[i].partial     = NULL;
        slub_caches[i].lock        = SPIN_INIT;
    }

    slub_initialized  = true;
    slub_initializing = false;

    spin_unlock(slub_init_lock);
    if (irq)
        arch_open_interrupt();
}

static bool slub_ensure_init(void) {
    if (likely(slub_initialized))
        return true;

    slub_init();
    return slub_initialized;
}

static int slub_cache_index_for(size_t size) {
    if (size > SIZE_MAX - SLUB_HEADER_SIZE)
        return -1;

    size_t needed = size + SLUB_HEADER_SIZE;
    if (needed > SIZE_MAX - (SLUB_ALIGN - 1))
        return -1;

    needed = slub_align_up(needed, SLUB_ALIGN);
    for (size_t i = 0; i < SLUB_CACHE_COUNT; i++) {
        if (needed <= slub_caches[i].object_size)
            return (int)i;
    }

    return -1;
}

static void slub_cache_lock(slub_cache_t *cache, bool *irq) {
    *irq = arch_check_interrupt();
    arch_close_interrupt();
    spin_lock(cache->lock);
}

static void slub_cache_unlock(slub_cache_t *cache, bool irq) {
    spin_unlock(cache->lock);
    if (irq)
        arch_open_interrupt();
}

static void slub_add_partial(slub_cache_t *cache, slub_slab_t *slab) {
    if (slab->on_partial)
        return;

    slab->prev = NULL;
    slab->next = cache->partial;
    if (cache->partial)
        cache->partial->prev = slab;
    cache->partial   = slab;
    slab->on_partial = true;
}

static void slub_remove_partial(slub_cache_t *cache, slub_slab_t *slab) {
    if (!slab->on_partial)
        return;

    if (slab->prev)
        slab->prev->next = slab->next;
    else
        cache->partial = slab->next;

    if (slab->next)
        slab->next->prev = slab->prev;

    slab->next       = NULL;
    slab->prev       = NULL;
    slab->on_partial = false;
}

static slub_slab_t *slub_grow_cache(slub_cache_t *cache) {
    uintptr_t phys = alloc_frames(cache->slab_pages);
    if (phys == 0)
        return NULL;

    uint8_t *mem = phys_to_virt(phys);
    if (!mem) {
        free_frames(phys, cache->slab_pages);
        return NULL;
    }

    memset(mem, 0, cache->slab_pages * PAGE_SIZE);

    slub_slab_t *slab = (slub_slab_t *)mem;
    uintptr_t start   = slub_align_up_ptr((uintptr_t)(slab + 1), SLUB_ALIGN);
    uintptr_t end     = (uintptr_t)mem + cache->slab_pages * PAGE_SIZE;

    if (end <= start || end - start < cache->object_size) {
        free_frames(phys, cache->slab_pages);
        return NULL;
    }

    size_t object_count = (end - start) / cache->object_size;
    if (object_count == 0 || object_count > UINT16_MAX) {
        free_frames(phys, cache->slab_pages);
        return NULL;
    }

    slab->cache         = cache;
    slab->phys          = phys;
    slab->total_objects = (uint16_t)object_count;
    slab->free_objects  = (uint16_t)object_count;
    slab->free_list     = NULL;
    slab->on_partial    = false;

    for (size_t i = 0; i < object_count; i++) {
        slub_free_node_t *node = (slub_free_node_t *)(start + i * cache->object_size);
        node->next             = slab->free_list;
        slab->free_list        = node;
    }

    return slab;
}

static void *slub_alloc_small(size_t size, int cache_index) {
    slub_cache_t *cache = &slub_caches[cache_index];

    for (;;) {
        bool irq = false;
        slub_cache_lock(cache, &irq);

        slub_slab_t *slab = cache->partial;
        if (slab && slab->free_list) {
            slub_free_node_t *node = slab->free_list;
            slab->free_list        = node->next;
            slab->free_objects--;

            if (slab->free_objects == 0)
                slub_remove_partial(cache, slab);

            slub_cache_unlock(cache, irq);

            slub_alloc_header_t *header = (slub_alloc_header_t *)node;
            header->magic               = SLUB_MAGIC;
            header->cache_index         = (uint16_t)cache_index;
            header->flags               = 0;
            header->requested_size      = size;
            header->slab                = slab;
            header->caller              = arch_get_return_address(1);
            header->alignment           = SLUB_ALIGN;

            return (uint8_t *)header + SLUB_HEADER_SIZE;
        }

        slub_cache_unlock(cache, irq);

        slab = slub_grow_cache(cache);
        if (!slab)
            return NULL;

        slub_cache_lock(cache, &irq);
        slub_add_partial(cache, slab);
        slub_cache_unlock(cache, irq);
    }
}

static size_t slub_round_page_count(size_t pages) {
    size_t rounded = 1;

    while (rounded < pages) {
        if (rounded > (SIZE_MAX >> 1))
            return 0;
        rounded <<= 1;
    }

    return rounded;
}

static void *slub_alloc_large(size_t size) {
    if (size > SIZE_MAX - SLUB_HEADER_SIZE)
        return NULL;

    size_t total = size + SLUB_HEADER_SIZE;
    if (total > SIZE_MAX - PAGE_SIZE + 1)
        return NULL;

    size_t pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
    pages        = slub_round_page_count(pages);
    if (pages == 0)
        return NULL;

    uintptr_t phys = alloc_frames(pages);
    if (phys == 0)
        return NULL;

    slub_alloc_header_t *header = phys_to_virt(phys);
    if (!header) {
        free_frames(phys, pages);
        return NULL;
    }

    header->magic          = SLUB_MAGIC;
    header->cache_index    = UINT16_MAX;
    header->flags          = SLUB_FLAG_LARGE;
    header->requested_size = size;
    header->pages          = pages;
    header->caller         = arch_get_return_address(1);
    header->alignment      = SLUB_ALIGN;

    return (uint8_t *)header + SLUB_HEADER_SIZE;
}

void *malloc(size_t size) {
    if (size == 0)
        return NULL;
    if (!slub_ensure_init())
        return NULL;

    int cache_index = slub_cache_index_for(size);
    if (cache_index >= 0)
        return slub_alloc_small(size, cache_index);

    return slub_alloc_large(size);
}

void *calloc(size_t nmemb, size_t size) {
    if (nmemb != 0 && size > SIZE_MAX / nmemb) {
        return NULL;
    }

    const size_t total = nmemb * size;
    void *ptr          = malloc(total);
    if (ptr)
        memset(ptr, 0, total);
    return ptr;
}

void *aligned_alloc(size_t alignment, size_t size) {
    if (size == 0)
        return NULL;
    if (!slub_is_power_of_two(alignment))
        return NULL;
    if (alignment <= SLUB_ALIGN)
        return malloc(size);

    if (size > SIZE_MAX - SLUB_HEADER_SIZE)
        return NULL;

    size_t extra = SLUB_HEADER_SIZE + alignment - 1;
    if (extra < SLUB_HEADER_SIZE || size > SIZE_MAX - extra)
        return NULL;

    void *raw_ptr = malloc(size + extra);
    if (!raw_ptr)
        return NULL;

    uintptr_t raw = (uintptr_t)raw_ptr;
    if (raw > UINTPTR_MAX - SLUB_HEADER_SIZE - (alignment - 1)) {
        free(raw_ptr);
        return NULL;
    }

    uintptr_t aligned           = slub_align_up_ptr(raw + SLUB_HEADER_SIZE, alignment);
    slub_alloc_header_t *header = (slub_alloc_header_t *)(aligned - SLUB_HEADER_SIZE);
    header->magic               = SLUB_MAGIC;
    header->cache_index         = UINT16_MAX;
    header->flags               = SLUB_FLAG_ALIGNED;
    header->requested_size      = size;
    header->raw_ptr             = raw_ptr;
    header->caller              = arch_get_return_address(1);
    header->alignment           = alignment;

    return (void *)aligned;
}

static slub_alloc_header_t *slub_header_from_ptr(void *ptr) {
    if (!ptr)
        return NULL;

    slub_alloc_header_t *header = (slub_alloc_header_t *)((uint8_t *)ptr - SLUB_HEADER_SIZE);
    if (header->magic != SLUB_MAGIC)
        return NULL;

    return header;
}

static size_t slub_capacity(slub_alloc_header_t *header) {
    if ((header->flags & SLUB_FLAG_ALIGNED) != 0)
        return header->requested_size;

    if ((header->flags & SLUB_FLAG_LARGE) != 0)
        return header->pages * PAGE_SIZE - SLUB_HEADER_SIZE;

    if (header->cache_index >= SLUB_CACHE_COUNT)
        return 0;

    return slub_caches[header->cache_index].object_size - SLUB_HEADER_SIZE;
}

void free(void *ptr) {
    slub_alloc_header_t *header = slub_header_from_ptr(ptr);
    if (!header)
        return;

    if ((header->flags & SLUB_FLAG_ALIGNED) != 0) {
        void *raw_ptr = header->raw_ptr;
        header->magic = 0;
        free(raw_ptr);
        return;
    }

    if ((header->flags & SLUB_FLAG_LARGE) != 0) {
        size_t pages  = header->pages;
        header->magic = 0;
        free_frames(virt_to_phys(header), pages);
        return;
    }

    if (header->cache_index >= SLUB_CACHE_COUNT)
        return;

    slub_slab_t *slab      = header->slab;
    slub_cache_t *cache    = &slub_caches[header->cache_index];
    slub_free_node_t *node = (slub_free_node_t *)header;
    bool release_slab      = false;

    bool irq = false;
    slub_cache_lock(cache, &irq);

    bool was_full = slab->free_objects == 0;
    if (was_full)
        slub_add_partial(cache, slab);

    node->next      = slab->free_list;
    slab->free_list = node;
    slab->free_objects++;

    if (slab->free_objects >= slab->total_objects) {
        slub_remove_partial(cache, slab);
        release_slab = true;
    }

    slub_cache_unlock(cache, irq);

    if (release_slab)
        free_frames(slab->phys, cache->slab_pages);
}

void *realloc(void *ptr, size_t size) {
    if (!ptr)
        return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    slub_alloc_header_t *header = slub_header_from_ptr(ptr);
    if (!header)
        return NULL;

    size_t capacity = slub_capacity(header);
    if (capacity >= size) {
        header->requested_size = size;
        return ptr;
    }

    size_t alignment = header->alignment;
    bool was_aligned = (header->flags & SLUB_FLAG_ALIGNED) != 0;
    void *new_ptr    = was_aligned ? aligned_alloc(alignment, size) : malloc(size);
    if (!new_ptr)
        return NULL;

    size_t copy_size = header->requested_size < size ? header->requested_size : size;
    memcpy(new_ptr, ptr, copy_size);
    free(ptr);

    return new_ptr;
}
