# extfs/lwext4/src/ext4.c

## `#define EXT4_MP_LOCK(_m) \ do {`

Mount point OS dependent lock

---

## `#define EXT4_MP_UNLOCK(_m) \ do {`

Mount point OS dependent unlock

---

## `struct ext4_mountpoint {`

Mount point descriptor.

---

## `bool mounted;`

Mount done flag.

---

## `char name[CONFIG_EXT4_MAX_MP_NAME + 1];`

Mount point name (@ref ext4_mount)

---

## `const struct ext4_lock *os_locks;`

OS dependent lock/unlock functions.

---

## `struct ext4_fs fs;`

Ext4 filesystem internals.

---

## `struct jbd_fs jbd_fs;`

JBD fs.

---

## `struct jbd_journal jbd_journal;`

Journal.

---

## `struct ext4_bcache bc;`

Block cache.

---

## `struct ext4_block_devices {`

Block devices descriptor.

---

## `char name[CONFIG_EXT4_MAX_BLOCKDEV_NAME + 1];`

Block device name.

---

## `struct ext4_blockdev *bd;`

Block device handle.

---

## `static struct ext4_block_devices s_bdevices[CONFIG_EXT4_BLOCKDEVS_COUNT];`

Block devices.

---

## `static struct ext4_mountpoint s_mp[CONFIG_EXT4_MOUNTPOINTS_COUNT];`

Mountpoints.

---

## `static bool ext4_is_dots(const uint8_t *name, size_t name_size) {`

************************************************************************

---

## `int ext4_mount(const char *dev_name, const char *mount_point, bool read_only) {`

************************************************************************

---

## `static int ext4_path_check(const char *path, bool *is_goal) {`

*****************************FILE OPERATIONS****************************

---

## `static int ext4_generic_open(ext4_file *f, const char *path, const char *flags, bool file_expect, uint32_t *parent_inode, uint32_t *name_off) {`

************************************************************************

---

## `int ext4_get_sblock(const char *mount_point, struct ext4_sblock **sb) {`

************************************************************************

---

## `int ext4_dir_rm(const char *path) {`

******************************DIRECTORY OPERATION***********************

---

