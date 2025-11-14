# extfs/lwext4/include/ext4_misc.h

## `#ifndef EXT4_MISC_H_ #define EXT4_MISC_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_misc.h
Miscellaneous helpers.


---

## `static inline uint64_t reorder64(uint64_t n) {`

*************************Endian conversion****************

---

## `#define ext4_get32(s, f) to_le32((s)->f) #define ext4_get16(s, f) to_le16((s)->f) #define ext4_get8(s, f) (s)->f #define ext4_set32(s, f, v) \ do \ {`

*************************Access macros to ext4 structures****************

---

## `#define jbd_get32(s, f) to_be32((s)->f) #define jbd_get16(s, f) to_be16((s)->f) #define jbd_get8(s, f) (s)->f #define jbd_set32(s, f, v) \ do \ {`

*************************Access macros to jbd2 structures****************

---

