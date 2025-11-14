# extfs/lwext4/src/ext4_hash.c

## `#include <ext4_config.h> #include <ext4_debug.h> #include <ext4_errno.h> #include <ext4_misc.h> #include <ext4_types.h> #include <fs_subsystem.h> /* F, G, and H are MD4 functions */ #define F(x, y, z) (((x) & (y)) | ((~x) & (z))) #define G(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z))) #define H(x, y, z) ((x) ^ (y) ^ (z)) /* ROTATE_LEFT rotates x left n bits */ #define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n)))) /* * FF, GG, and HH are transformations for rounds 1, 2, and 3. * Rotation is separated from addition to prevent recomputation. */ #define FF(a, b, c, d, x, s) \ {`


@file  ext4_hash.c
Directory indexing hash functions.


---

