# extfs/lwext4/include/ext4_types.h

## `#ifndef EXT4_TYPES_H_ #define EXT4_TYPES_H_ #ifdef __cplusplus extern "C" {`


@file  ext4_types.h
Ext4 data structure definitions.


---

## `enum {`

Directory entry types. 

---

## `struct ext4_dir_en {`


Linked list directory entry structure


---

## `/* * JBD stores integers in big endian. */ #define JBD_MAGIC_NUMBER 0xc03b3998U /* The first 4 bytes of /dev/random! */ /* * Descriptor block types: */ #define JBD_DESCRIPTOR_BLOCK 1 #define JBD_COMMIT_BLOCK 2 #define JBD_SUPERBLOCK 3 #define JBD_SUPERBLOCK_V2 4 #define JBD_REVOKE_BLOCK 5 #pragma pack(push, 1) /* * Standard header for all descriptor blocks: */ struct jbd_bhdr {`

*************************************************************************

---

