# extfs/lwext4/include/ext4_hash.h

## `#ifndef EXT4_HASH_H_ #define EXT4_HASH_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_hash.h
Directory indexing hash functions.


---

## `int ext2_htree_hash(const char *name, int len, const uint32_t *hash_seed, int hash_version, uint32_t *hash_major, uint32_t *hash_minor);`

Directory entry name hash function.

- **`name`**: entry name
- **`len`**: entry name length
- **`hash_seed`**: (from superblock)
- **`hash_version`**: version (from superblock)
- **`hash_minor`**: output value
- **`hash_major`**: output value
- **Returns**: standard error code

---

