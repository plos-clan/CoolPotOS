# include/lib/libfdt/libfdt_internal.h

## `#ifndef FDT_ASSUME_MASK #define FDT_ASSUME_MASK 0 #endif /* * Defines assumptions which can be enabled. Each of these can be enabled * individually. For maximum safety, don't enable any assumptions! * * For minimal code size and no safety, use ASSUME_PERFECT at your own risk. * You should have another method of validating the device tree, such as a * signature or hash check before using libfdt. * * For situations where security is not a concern it may be safe to enable * ASSUME_SANE. */ enum {`

******************************************************************

---

## `static inline bool can_assume_(int mask) {`


can_assume_() - check if a particular assumption is enabled

@mask: Mask to check (ASSUME_...)

- **Returns**: true if that assumption is enabled, else false


---

