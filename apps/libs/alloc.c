// This code is released under the MIT License

#include "../include/alloc.h"
#include "../include/syscall.h"


static struct mman mman;

void *malloc(size_t size) {
   // return mman_alloc(&mman, size);
    return syscall_malloc(size);
}

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == null) exit(1);
    return ptr;
}

void free(void *ptr) {
    //mman_free(&mman, ptr);
    syscall_free(ptr);
}

void *calloc(size_t n, size_t size) {
    void *ptr = malloc(n * size);
    if (ptr == null) return null;
    memset(ptr, 0, n * size);
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    //return mman_realloc(&mman, ptr, newsize);

    void *new = malloc(size);
    if (ptr) {
        memcpy(new, ptr, *(int *)((int)ptr - 4));
        free(ptr);
    }
    return new;
}

void *reallocarray(void *ptr, size_t n, size_t size) {
    return realloc(ptr, n * size);
}

void *aligned_alloc(size_t align, size_t size) { // 这分配的不能释放，别用
    if (!(align & (align - 1)) && align <= 2 * sizeof(size_t)) return mman_alloc(&mman, size);
    void *ptr = malloc(size + align - 1);
    if (ptr == null) return null;
    ptr = (ptr + align - 1) - (size_t) (ptr + align - 1) % align;
    return ptr;
}

size_t malloc_usable_size(void *ptr) {
    return mman_msize(&mman, ptr);
}

void *memalign(size_t align, size_t size) { // 这分配的不能释放，别用
    return aligned_alloc(align, size);
}

int posix_memalign(void **memptr, size_t alignment, size_t size) { // 这分配的不能释放，别用
    void *ptr = aligned_alloc(alignment, size);
    if (ptr == null) return 1;
    *memptr = ptr;
    return 0;
}

void *pvalloc(size_t size) { // 这分配的不能释放，别用
    return aligned_alloc(4096, size);
}

void *valloc(size_t size) { // 这分配的不能释放，别用
    return aligned_alloc(4096, size);
}

static void *mmap_buf_4k[4] = {null};
static void *mmap_buf_2M = null;

static void *wrap_mmap(void *addr, size_t size) {
    if (size == SIZE_2M) {
        if (mmap_buf_2M != null) {
            void *tmp = mmap_buf_2M;
            mmap_buf_2M = null;
            memset(tmp, 0, SIZE_2M);
            return tmp;
        }
        return mmap(addr, size);
    }
    if (size == SIZE_4k) {
#pragma unroll
        for (int i = 0; i < 4; i++) {
            if (mmap_buf_4k[i] != null) {
                void *tmp = mmap_buf_4k[i];
                mmap_buf_4k[i] = null;
                memset(tmp, 0, SIZE_4k);
                return tmp;
            }
        }
        return mmap(addr, size);
    }
    return mmap(addr, size);
}

static void wrap_munmap(void *addr, size_t size) {
    if (size == SIZE_2M) {
        if (mmap_buf_2M == null) {
            mmap_buf_2M = addr;
            return;
        }
        munmap(addr, size);
        return;
    }
    if (size == SIZE_4k) {
#pragma unroll
        for (int i = 0; i < 4; i++) {
            if (mmap_buf_4k[i] == null) {
                mmap_buf_4k[i] = addr;
                return;
            }
        }
        munmap(addr, size);
        return;
    }
    munmap(addr, size);
}

extern int printf(const char* fmt,...);

bool __libc_init_mman() {
    void *ptr = mmap(null, SIZE_4k);
    if (ptr == null) return false;
    mman_init(&mman, ptr, SIZE_4k);
    mman_setcb(&mman, wrap_mmap, wrap_munmap);
    return true;
}
