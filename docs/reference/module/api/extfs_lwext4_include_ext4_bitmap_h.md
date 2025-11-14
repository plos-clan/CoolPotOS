# extfs/lwext4/include/ext4_bitmap.h

## `#ifndef EXT4_BITMAP_H_ #define EXT4_BITMAP_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_bitmap.h
Block/inode bitmap allocator.


---

## `static inline void ext4_bmap_bit_set(uint8_t *bmap, uint32_t bit) {`

Set bitmap bit.

- **`bmap`**: bitmap
- **`bit`**: bit to set

---

## `static inline void ext4_bmap_bit_clr(uint8_t *bmap, uint32_t bit) {`

Clear bitmap bit.

- **`bmap`**: bitmap buffer
- **`bit`**: bit to clear

---

## `static inline bool ext4_bmap_is_bit_set(uint8_t *bmap, uint32_t bit) {`

Check if the bitmap bit is set.

- **`bmap`**: bitmap buffer
- **`bit`**: bit to check

---

## `static inline bool ext4_bmap_is_bit_clr(uint8_t *bmap, uint32_t bit) {`

Check if the bitmap bit is clear.

- **`bmap`**: bitmap buffer
- **`bit`**: bit to check

---

## `void ext4_bmap_bits_free(uint8_t *bmap, uint32_t sbit, uint32_t bcnt);`

Free range of bits in bitmap.

- **`bmap`**: bitmap buffer
- **`sbit`**: start bit
- **`bcnt`**: bit count

---

## `int ext4_bmap_bit_find_clr(uint8_t *bmap, uint32_t sbit, uint32_t ebit, uint32_t *bit_id);`

Find first clear bit in bitmap.

- **`sbit`**: start bit of search
- **`ebit`**: end bit of search
- **`bit_id`**: output parameter (first free bit)
- **Returns**: standard error code

---

