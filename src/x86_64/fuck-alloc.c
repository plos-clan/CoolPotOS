
#include <io.h>
#include <lock.h>
#include <stddef.h>
#include <stdint.h>

#undef malloc
#undef free
#undef get_allocator_size
#undef realloc

static const size_t canaria[] = {
    0x1939e9d943699c68, 0xf03a1b212bd97bc8, 0x80bd66e7a068490b, 0x5e0d8f05b214ce1c,
    0xa4ba0cde51430b7a, 0x28b1e8e2840fb599, 0x25da605a676e0807, 0xed610c49c9f8db02,
};

static size_t *_malloc_rng(size_t *buf, size_t len) {
    static size_t seed = 0x0d0007210d000721;
    for (size_t i = 0; i < len; i++) {
        seed   = seed * 0x2e624114b6b255df + 0xb4f1e438a94f24a;
        buf[i] = seed + 0x290c2134746e4c5c;
    }
    return buf;
}

void *_real_malloc(size_t size);

void _real_free(void *ptr);

void *_real_realloc(void *ptr, size_t size);

#define PORT 0x3f8

static void _putb(int c) {
    while (!(io_in8(PORT + 5) & 0x20)) {}
    io_out8(PORT, c);
}

static void _puts(const char *s) {
    for (size_t i = 0; s[i] != '\0'; i++) {
        _putb(s[i]);
    }
}

static void _canaria_panic() {
    while (true) {
        __asm__ volatile("cli");
        _puts("\n"
              "===== ===== ===== ===== ===== ===== ===== =====\n"
              "Canaria panic: Memory corruption detected!\n"
              "Please report this issue to the developers.\n");
        __asm__ volatile("hlt");
    }
}

static void  *alloced_mem[8] = {};
static size_t nballocs       = 0;
static spin_t malloc_lock;

#define alloced_mem_entry (alloced_mem[nballocs++ % (sizeof(alloced_mem) / sizeof(*alloced_mem))])

void _fuck_check(void *ptr) {
    size_t *data = (size_t *)ptr - 4;
    if (data[0] != canaria[0]) _canaria_panic();
    if (data[1] != canaria[1]) _canaria_panic();
    size_t len1 = data[2] ^ canaria[2];
    size_t len2 = data[3] ^ canaria[3];
    if (len1 != len2) _canaria_panic();
    size_t len = len1;
    if (data[len - 4] != (canaria[4] ^ len)) _canaria_panic();
    if (data[len - 3] != (canaria[5] ^ len)) _canaria_panic();
    if (data[len - 2] != canaria[6]) _canaria_panic();
    if (data[len - 1] != canaria[7]) _canaria_panic();
}

void *_fuck_malloc(size_t size) {
    const bool _is_sti_ = ({
                              size_t flags;
                              __asm__ volatile("pushf\n\t"
                                               "pop %0\n\t"
                                               : "=r"(flags)
                                               :);
                              flags;
                          }) &
                          (1 << 9);
    __asm__ volatile("cli");
    spin_lock(malloc_lock);

    for (size_t i = 0; i < sizeof(alloced_mem) / sizeof(*alloced_mem); i++) {
        if (alloced_mem[i]) _fuck_check(alloced_mem[i]);
    }
    size_t  len = (size + sizeof(size_t) - 1) / sizeof(size_t) + 8;
    size_t *ptr = _real_malloc(len * sizeof(size_t));
    if (ptr == NULL) {
        spin_unlock(malloc_lock);
        if (_is_sti_) __asm__ volatile("sti");
        return NULL;
    }
    ptr[0]       = canaria[0];
    ptr[1]       = canaria[1];
    ptr[2]       = canaria[2] ^ len;
    ptr[3]       = canaria[3] ^ len;
    ptr[len - 4] = canaria[4] ^ len;
    ptr[len - 3] = canaria[5] ^ len;
    ptr[len - 2] = canaria[6];
    ptr[len - 1] = canaria[7];
    void *addr = alloced_mem_entry = _malloc_rng(ptr + 4, len - 8);

    spin_unlock(malloc_lock);
    if (_is_sti_) __asm__ volatile("sti");
    return addr;
}

void _fuck_free(void *ptr) {
    const bool _is_sti_ = ({
                              size_t flags;
                              __asm__ volatile("pushf\n\t"
                                               "pop %0\n\t"
                                               : "=r"(flags)
                                               :);
                              flags;
                          }) &
                          (1 << 9);
    __asm__ volatile("cli");
    spin_lock(malloc_lock);

    size_t *data = (size_t *)ptr - 4;
    if (data[0] != canaria[0]) _canaria_panic();
    if (data[1] != canaria[1]) _canaria_panic();
    size_t len1 = data[2] ^ canaria[2];
    size_t len2 = data[3] ^ canaria[3];
    if (len1 != len2) _canaria_panic();
    size_t len = len1;
    if (data[len - 4] != (canaria[4] ^ len)) _canaria_panic();
    if (data[len - 3] != (canaria[5] ^ len)) _canaria_panic();
    if (data[len - 2] != canaria[6]) _canaria_panic();
    if (data[len - 1] != canaria[7]) _canaria_panic();
    _real_free(data);
    for (size_t i = 0; i < sizeof(alloced_mem) / sizeof(*alloced_mem); i++) {
        if (alloced_mem[i] == ptr) {
            alloced_mem[i] = NULL;
            break;
        }
    }

    spin_unlock(malloc_lock);
    if (_is_sti_) __asm__ volatile("sti");
}

size_t _fuck_usable_size(void *ptr) {
    const bool _is_sti_ = ({
                              size_t flags;
                              __asm__ volatile("pushf\n\t"
                                               "pop %0\n\t"
                                               : "=r"(flags)
                                               :);
                              flags;
                          }) &
                          (1 << 9);
    __asm__ volatile("cli");
    spin_lock(malloc_lock);

    size_t *data = (size_t *)ptr - 4;
    if (data[0] != canaria[0]) _canaria_panic();
    if (data[1] != canaria[1]) _canaria_panic();
    size_t len1 = data[2] ^ canaria[2];
    size_t len2 = data[3] ^ canaria[3];
    if (len1 != len2) _canaria_panic();
    size_t len = len1;
    if (data[len - 4] != (canaria[4] ^ len)) _canaria_panic();
    if (data[len - 3] != (canaria[5] ^ len)) _canaria_panic();
    if (data[len - 2] != canaria[6]) _canaria_panic();
    if (data[len - 1] != canaria[7]) _canaria_panic();

    spin_unlock(malloc_lock);
    if (_is_sti_) __asm__ volatile("sti");
    return (len - 8) * sizeof(size_t);
}

void *_fuck_realloc(void *ptr, size_t size) {
    const bool _is_sti_ = ({
                              size_t flags;
                              __asm__ volatile("pushf\n\t"
                                               "pop %0\n\t"
                                               : "=r"(flags)
                                               :);
                              flags;
                          }) &
                          (1 << 9);
    __asm__ volatile("cli");
    spin_lock(malloc_lock);

    for (size_t i = 0; i < sizeof(alloced_mem) / sizeof(*alloced_mem); i++) {
        if (alloced_mem[i]) _fuck_check(alloced_mem[i]);
    }
    size_t *data = (size_t *)ptr - 4;
    if (data[0] != canaria[0]) _canaria_panic();
    if (data[1] != canaria[1]) _canaria_panic();
    size_t len1 = data[2] ^ canaria[2];
    size_t len2 = data[3] ^ canaria[3];
    if (len1 != len2) _canaria_panic();
    size_t len = len1;
    if (data[len - 4] != (canaria[4] ^ len)) _canaria_panic();
    if (data[len - 3] != (canaria[5] ^ len)) _canaria_panic();
    if (data[len - 2] != canaria[6]) _canaria_panic();
    if (data[len - 1] != canaria[7]) _canaria_panic();
    size_t new_len = (size + sizeof(size_t) - 1) / sizeof(size_t) + 8;
    size_t *new    = _real_realloc(data, new_len);
    if (new == NULL) {
        spin_unlock(malloc_lock);
        if (_is_sti_) __asm__ volatile("sti");
        return NULL;
    }
    for (size_t i = 0; i < sizeof(alloced_mem) / sizeof(*alloced_mem); i++) {
        if (alloced_mem[i] == ptr) {
            alloced_mem[i] = NULL;
            break;
        }
    }
    new[0]           = canaria[0];
    new[1]           = canaria[1];
    new[2]           = canaria[2] ^ new_len;
    new[3]           = canaria[3] ^ new_len;
    new[new_len - 4] = canaria[4] ^ new_len;
    new[new_len - 3] = canaria[5] ^ new_len;
    new[new_len - 2] = canaria[6];
    new[new_len - 1] = canaria[7];
    if (new_len > len) _malloc_rng(new + len - 4, new_len - len);
    void *addr = alloced_mem_entry = new + 4;

    spin_unlock(malloc_lock);
    if (_is_sti_) __asm__ volatile("sti");
    return addr;
}
