# extfs/lwext4/include/ext4_crc32.h

## `#ifndef LWEXT4_EXT4_CRC32C_H_ #define LWEXT4_EXT4_CRC32C_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_crc32.h
Crc32c routine. Taken from FreeBSD kernel.


---

## `uint32_t ext4_crc32(uint32_t crc, const void *buf, uint32_t size);`

CRC32 algorithm.

- **`crc`**: input feed
- **`buf`**: input buffer
- **`size`**: input buffer length (bytes)
- **Returns**: updated crc32 value

---

## `uint32_t ext4_crc32c(uint32_t crc, const void *buf, uint32_t size);`

CRC32C algorithm.

- **`crc`**: input feed
- **`buf`**: input buffer
- **`size`**: input buffer length (bytes)
- **Returns**: updated crc32c value

---

