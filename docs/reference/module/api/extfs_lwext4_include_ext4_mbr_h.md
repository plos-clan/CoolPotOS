# extfs/lwext4/include/ext4_mbr.h

## `#ifndef EXT4_MBR_H_ #define EXT4_MBR_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_mbr.h
Master boot record parser


---

## `struct ext4_mbr_bdevs {`

Master boot record block devices descriptor

---

## `struct ext4_mbr_parts {`

Master boot record partitions

---

## `uint8_t division[4];`

Percentage division tab:
- {50, 20, 10, 20}
Sum of all 4 elements must be <= 100

---

