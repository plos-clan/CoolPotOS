# lib/zstd/common/compiler.h

## `#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ >= 4 && __GNUC_MINOR__ >= 8 && __GNUC__ < 5 # define HINT_INLINE static INLINE_KEYWORD #else # define HINT_INLINE static INLINE_KEYWORD FORCE_INLINE_ATTR #endif /* UNUSED_ATTR tells the compiler it is okay if the function is unused. */ #if defined(__GNUC__) # define UNUSED_ATTR __attribute__((unused)) #else # define UNUSED_ATTR #endif /* force no inlining */ #ifdef _MSC_VER # define FORCE_NOINLINE static __declspec(noinline) #else # if defined(__GNUC__) || defined(__ICCARM__) # define FORCE_NOINLINE static __attribute__((__noinline__)) # else # define FORCE_NOINLINE static # endif #endif /* target attribute */ #if defined(__GNUC__) || defined(__ICCARM__) # define TARGET_ATTRIBUTE(target) __attribute__((__target__(target))) #else # define TARGET_ATTRIBUTE(target) #endif /* Target attribute for BMI2 dynamic dispatch. * Enable lzcnt, bmi, and bmi2. * We test for bmi1 & bmi2. lzcnt is included in bmi1. */ #define BMI2_TARGET_ATTRIBUTE TARGET_ATTRIBUTE("lzcnt,bmi,bmi2") /* prefetch * can be disabled, by declaring NO_PREFETCH build macro */ #if defined(NO_PREFETCH) # define PREFETCH_L1(ptr) (void)(ptr) /* disabled */ # define PREFETCH_L2(ptr) (void)(ptr) /* disabled */ #else # if defined(_MSC_VER) && \ (defined(_M_X64) || \ defined(_M_I86)) /* _mm_prefetch() is not defined outside of x86/x64 */ # include <mmintrin.h> /* https://msdn.microsoft.com/fr-fr/library/84szxsww(v=vs.90).aspx */ # define PREFETCH_L1(ptr) _mm_prefetch((const char *)(ptr), _MM_HINT_T0) # define PREFETCH_L2(ptr) _mm_prefetch((const char *)(ptr), _MM_HINT_T1) # elif defined(__GNUC__) && ((__GNUC__ >= 4) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1))) # define PREFETCH_L1(ptr) __builtin_prefetch((ptr), 0 /* rw==read */, 3 /* locality */) # define PREFETCH_L2(ptr) __builtin_prefetch((ptr), 0 /* rw==read */, 2 /* locality */) # elif defined(__aarch64__) # define PREFETCH_L1(ptr) __asm__ __volatile__("prfm pldl1keep, %0" ::"Q"(*(ptr))) # define PREFETCH_L2(ptr) __asm__ __volatile__("prfm pldl2keep, %0" ::"Q"(*(ptr))) # else # define PREFETCH_L1(ptr) (void)(ptr) /* disabled */ # define PREFETCH_L2(ptr) (void)(ptr) /* disabled */ # endif #endif /* NO_PREFETCH */ #define CACHELINE_SIZE 64 #define PREFETCH_AREA(p, s) \ {`


HINT_INLINE is used to help the compiler generate better code. It is *not*
used for "templates", so it can be tweaked based on the compilers
performance.

gcc-4.8 and gcc-4.9 have been shown to benefit from leaving off the
always_inline attribute.

clang up to 5.0.0 (trunk) benefit tremendously from the always_inline
attribute.


---

## `void __asan_poison_memory_region(void const volatile *addr, size_t size);`


Marks a memory region (<c>[addr, addr+size)</c>) as unaddressable.

This memory must be previously allocated by your program. Instrumented
code is forbidden from accessing addresses in this region until it is
unpoisoned. This function is not guaranteed to poison the entire region -
it could poison only a subregion of <c>[addr, addr+size)</c> due to ASan
alignment restrictions.

\note This function is not thread-safe because no two threads can poison or
unpoison memory in the same memory region simultaneously.

\param addr Start of memory region.
\param size Size of memory region. 

---

## `void __asan_unpoison_memory_region(void const volatile *addr, size_t size);`


Marks a memory region (<c>[addr, addr+size)</c>) as addressable.

This memory must be previously allocated by your program. Accessing
addresses in this region is allowed until this region is poisoned again.
This function could unpoison a super-region of <c>[addr, addr+size)</c> due
to ASan alignment restrictions.

\note This function is not thread-safe because no two threads can
poison or unpoison memory in the same memory region simultaneously.

\param addr Start of memory region.
\param size Size of memory region. 

---

