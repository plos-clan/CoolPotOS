# extfs/lwext4/include/ext4_debug.h

## `#ifndef EXT4_DEBUG_H_ #define EXT4_DEBUG_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_debug.c
Debug printf and assert macros.


---

## `void ext4_dmask_set(uint32_t m);`

Global mask debug set.

- **`m`**: new debug mask.

---

## `void ext4_dmask_clr(uint32_t m);`

Global mask debug clear.

- **`m`**: new debug mask.

---

## `uint32_t ext4_dmask_get(void);`

Global debug mask get.

- **Returns**: debug mask

---

## `#define ext4_dbg(m, ...) \ do \ {`

Debug printf.

---

## `#if CONFIG_HAVE_OWN_ASSERT #include <stdio.h> #define ext4_assert(_v) \ do \ {`

Debug assertion.

---

