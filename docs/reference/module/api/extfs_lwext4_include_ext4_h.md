# extfs/lwext4/include/ext4.h

## `#ifndef EXT4_H_ #define EXT4_H_ #ifdef __cplusplus extern "C" {`


@file  ext4.h
Ext4 high level operations (files, directories, mount points...).
Client has to include only this file.


---

## `struct ext4_lock {`

OS dependent lock interface.

---

## `void (*lock)(void);`

Lock access to mount point.

---

## `void (*unlock)(void);`

Unlock access to mount point.

---

## `typedef struct ext4_file {`

File descriptor. 

---

## `struct ext4_mountpoint *mp;`

Mount point handle.

---

## `uint32_t inode;`

File inode id.

---

## `uint32_t flags;`

Open flags.

---

## `uint64_t fsize;`

File size.

---

## `uint64_t fpos;`

Actual file position.

---

## `typedef struct ext4_direntry {`

Directory entry descriptor. 

---

## `typedef struct ext4_dir {`

Directory descriptor. 

---

## `ext4_file f;`

File descriptor.

---

## `ext4_direntry de;`

Current directory entry.

---

## `uint64_t next_off;`

Next entry offset.

---

## `int ext4_device_register(struct ext4_blockdev *bd, const char *dev_name);`

Register block device.


- **`bd`**: Block device.
- **`dev_name`**: Block device name.


- **Returns**: Standard error code.

---

## `int ext4_device_unregister(const char *dev_name);`

Un-register block device.


- **`dev_name`**: Block device name.


- **Returns**: Standard error code.

---

## `int ext4_device_unregister_all(void);`

Un-register all block devices.


- **Returns**: Standard error code.

---

## `int ext4_mount(const char *dev_name, const char *mount_point, bool read_only);`

Mount a block device with EXT4 partition to the mount point.


- **`dev_name`**: Block device name (@ref ext4_device_register).
- **`mount_point`**: Mount point, for example:
-   /
-   /my_partition/
-   /my_second_partition/

- **`read_only`**: mount as read-only mode.


- **Returns**: Standard error code 

---

## `int ext4_umount(const char *mount_point);`

Umount operation.


- **`mount_point`**: Mount point.


- **Returns**: Standard error code 

---

## `int ext4_journal_start(const char *mount_point);`

Starts journaling. Journaling start/stop functions are transparent
and might be used on filesystems without journaling support.
@warning Usage:
ext4_mount("sda1", "/");
ext4_journal_start("/");

//File operations here...

ext4_journal_stop("/");
ext4_umount("/");

- **`mount_point`**: Mount point.


- **Returns**: Standard error code. 

---

## `int ext4_journal_stop(const char *mount_point);`

Stops journaling. Journaling start/stop functions are transparent
and might be used on filesystems without journaling support.


- **`mount_point`**: Mount point name.


- **Returns**: Standard error code. 

---

## `int ext4_recover(const char *mount_point);`

Journal recovery.
@warning Must be called after @ref ext4_mount.


- **`mount_point`**: Mount point.


- **Returns**: Standard error code. 

---

## `struct ext4_mount_stats {`

Some of the filesystem stats. 

---

## `int ext4_mount_point_stats(const char *mount_point, struct ext4_mount_stats *stats);`

Get file mount point stats.


- **`mount_point`**: Mount point.
- **`stats`**: Filesystem stats.


- **Returns**: Standard error code. 

---

## `int ext4_mount_setup_locks(const char *mount_point, const struct ext4_lock *locks);`

Setup OS lock routines.


- **`mount_point`**: Mount point.
- **`locks`**: Lock and unlock functions


- **Returns**: Standard error code. 

---

## `int ext4_get_sblock(const char *mount_point, struct ext4_sblock **sb);`

Acquire the filesystem superblock pointer of a mp.


- **`mount_point`**: Mount point.
- **`sb`**: Superblock handle


- **Returns**: Standard error code. 

---

## `int ext4_cache_write_back(const char *path, bool on);`

Enable/disable write back cache mode.
@warning Default model of cache is write trough. It means that when You do:

ext4_fopen(...);
ext4_fwrite(...);
< --- data is flushed to physical drive

When you do:
ext4_cache_write_back(..., 1);
ext4_fopen(...);
ext4_fwrite(...);
< --- data is NOT flushed to physical drive
ext4_cache_write_back(..., 0);
< --- when write back mode is disabled all
cache data will be flushed
To enable write back mode permanently just call this function
once after ext4_mount (and disable before ext4_umount).

Some of the function use write back cache mode internally.
If you enable write back mode twice you have to disable it twice
to flush all data:

ext4_cache_write_back(..., 1);
ext4_cache_write_back(..., 1);

ext4_cache_write_back(..., 0);
ext4_cache_write_back(..., 0);

Write back mode is useful when you want to create a lot of empty
files/directories.


- **`path`**: Mount point.
- **`on`**: Enable/disable cache writeback mode.


- **Returns**: Standard error code. 

---

## `int ext4_cache_flush(const char *path);`

Force cache flush.


- **`path`**: Mount point.


- **Returns**: Standard error code. 

---

## `int ext4_fremove(const char *path);`

Remove file by path.


- **`path`**: Path to file.


- **Returns**: Standard error code. 

---

## `int ext4_flink(const char *path, const char *hardlink_path);`

Create a hardlink for a file.


- **`path`**: Path to file.
- **`hardlink_path`**: Path of hardlink.


- **Returns**: Standard error code. 

---

## `int ext4_frename(const char *path, const char *new_path);`

Rename file.

- **`path`**: Source.
- **`new_path`**: Destination.
- **Returns**: Standard error code. 

---

## `int ext4_fopen(ext4_file *file, const char *path, const char *flags);`

File open function.


- **`file`**: File handle.
- **`path`**: File path, has to start from mount point:/my_partition/file.
- **`flags`**: File open flags.
|---------------------------------------------------------------|
|   r or rb                 O_RDONLY                            |
|---------------------------------------------------------------|
|   w or wb                 O_WRONLY|O_CREAT|O_TRUNC            |
|---------------------------------------------------------------|
|   a or ab                 O_WRONLY|O_CREAT|O_APPEND           |
|---------------------------------------------------------------|
|   r+ or rb+ or r+b        O_RDWR                              |
|---------------------------------------------------------------|
|   w+ or wb+ or w+b        O_RDWR|O_CREAT|O_TRUNC              |
|---------------------------------------------------------------|
|   a+ or ab+ or a+b        O_RDWR|O_CREAT|O_APPEND             |
|---------------------------------------------------------------|


- **Returns**: Standard error code.

---

## `int ext4_fopen2(ext4_file *file, const char *path, int flags);`

Alternate file open function.


- **`file`**: File handle.
- **`path`**: File path, has to start from mount point:/my_partition/file.
- **`flags`**: File open flags.


- **Returns**: Standard error code.

---

## `int ext4_fclose(ext4_file *file);`

File close function.


- **`file`**: File handle.


- **Returns**: Standard error code.

---

## `int ext4_ftruncate(ext4_file *file, uint64_t size);`

File truncate function.


- **`file`**: File handle.
- **`size`**: New file size.


- **Returns**: Standard error code.

---

## `int ext4_fread(ext4_file *file, void *buf, size_t size, size_t *rcnt);`

Read data from file.


- **`file`**: File handle.
- **`buf`**: Output buffer.
- **`size`**: Bytes to read.
- **`rcnt`**: Bytes read (NULL allowed).


- **Returns**: Standard error code.

---

## `int ext4_fwrite(ext4_file *file, const void *buf, size_t size, size_t *wcnt);`

Write data to file.


- **`file`**: File handle.
- **`buf`**: Data to write
- **`size`**: Write length..
- **`wcnt`**: Bytes written (NULL allowed).


- **Returns**: Standard error code.

---

## `int ext4_fseek(ext4_file *file, int64_t offset, uint32_t origin);`

File seek operation.


- **`file`**: File handle.
- **`offset`**: Offset to seek.
- **`origin`**: Seek type:
@ref SEEK_SET
@ref SEEK_CUR
@ref SEEK_END


- **Returns**: Standard error code.

---

## `uint64_t ext4_ftell(ext4_file *file);`

Get file position.


- **`file`**: File handle.


- **Returns**: Actual file position 

---

## `uint64_t ext4_fsize(ext4_file *file);`

Get file size.


- **`file`**: File handle.


- **Returns**: File size. 

---

## `int ext4_raw_inode_fill(const char *path, uint32_t *ret_ino, struct ext4_inode *inode);`

Get inode of file/directory/link.


- **`path`**: Parh to file/dir/link.
- **`ret_ino`**: Inode number.
- **`inode`**: Inode internals.


- **Returns**: Standard error code.

---

## `int ext4_inode_exist(const char *path, int type);`

Check if inode exists.


- **`path`**: Parh to file/dir/link.
- **`type`**: Inode type.
@ref EXT4_DIRENTRY_UNKNOWN
@ref EXT4_DE_REG_FILE
@ref EXT4_DE_DIR
@ref EXT4_DE_CHRDEV
@ref EXT4_DE_BLKDEV
@ref EXT4_DE_FIFO
@ref EXT4_DE_SOCK
@ref EXT4_DE_SYMLINK


- **Returns**: Standard error code.

---

## `int ext4_mode_set(const char *path, uint32_t mode);`

Change file/directory/link mode bits.


- **`path`**: Path to file/dir/link.
- **`mode`**: New mode bits (for example 0777).


- **Returns**: Standard error code.

---

## `int ext4_mode_get(const char *path, uint32_t *mode);`

Get file/directory/link mode bits.


- **`path`**: Path to file/dir/link.
- **`mode`**: New mode bits (for example 0777).


- **Returns**: Standard error code.

---

## `int ext4_owner_set(const char *path, uint32_t uid, uint32_t gid);`

Change file owner and group.


- **`path`**: Path to file/dir/link.
- **`uid`**: User id.
- **`gid`**: Group id.


- **Returns**: Standard error code.

---

## `int ext4_owner_get(const char *path, uint32_t *uid, uint32_t *gid);`

Get file/directory/link owner and group.


- **`path`**: Path to file/dir/link.
- **`uid`**: User id.
- **`gid`**: Group id.


- **Returns**: Standard error code.

---

## `int ext4_atime_set(const char *path, uint32_t atime);`

Set file/directory/link access time.


- **`path`**: Path to file/dir/link.
- **`atime`**: Access timestamp.


- **Returns**: Standard error code.

---

## `int ext4_mtime_set(const char *path, uint32_t mtime);`

Set file/directory/link modify time.


- **`path`**: Path to file/dir/link.
- **`mtime`**: Modify timestamp.


- **Returns**: Standard error code.

---

## `int ext4_ctime_set(const char *path, uint32_t ctime);`

Set file/directory/link change time.


- **`path`**: Path to file/dir/link.
- **`ctime`**: Change timestamp.


- **Returns**: Standard error code.

---

## `int ext4_atime_get(const char *path, uint32_t *atime);`

Get file/directory/link access time.


- **`path`**: Path to file/dir/link.
- **`atime`**: Access timestamp.


- **Returns**: Standard error code.

---

## `int ext4_mtime_get(const char *path, uint32_t *mtime);`

Get file/directory/link modify time.


- **`path`**: Path to file/dir/link.
- **`mtime`**: Modify timestamp.


- **Returns**: Standard error code.

---

## `int ext4_ctime_get(const char *path, uint32_t *ctime);`

Get file/directory/link change time.


- **`path`**: Pathto file/dir/link.
- **`ctime`**: Change timestamp.


- **Returns**: standard error code

---

## `int ext4_fsymlink(const char *target, const char *path);`

Create symbolic link.


- **`target`**: Destination entry path.
- **`path`**: Source entry path.


- **Returns**: Standard error code.

---

## `int ext4_mknod(const char *path, int filetype, uint32_t dev);`

Create special file.

- **`path`**: Path to new special file.
- **`filetype`**: Filetype of the new special file.
(that must not be regular file, directory, or unknown type)

- **`dev`**: If filetype is char device or block device,
the device number will become the payload in the inode.

- **Returns**: Standard error code.

---

## `int ext4_readlink(const char *path, char *buf, size_t bufsize, size_t *rcnt);`

Read symbolic link payload.


- **`path`**: Path to symlink.
- **`buf`**: Output buffer.
- **`bufsize`**: Output buffer max size.
- **`rcnt`**: Bytes read.


- **Returns**: Standard error code.

---

## `int ext4_setxattr(const char *path, const char *name, size_t name_len, const void *data, size_t data_size);`

Set extended attribute.


- **`path`**: Path to file/directory
- **`name`**: Name of the entry to add.
- **`name_len`**: Length of @name in bytes.
- **`data`**: Data of the entry to add.
- **`data_size`**: Size of data to add.


- **Returns**: Standard error code.

---

## `int ext4_getxattr(const char *path, const char *name, size_t name_len, void *buf, size_t buf_size, size_t *data_size);`

Get extended attribute.


- **`path`**: Path to file/directory.
- **`name`**: Name of the entry to get.
- **`name_len`**: Length of @name in bytes.
- **`buf`**: Data of the entry to get.
- **`buf_size`**: Size of data to get.


- **Returns**: Standard error code.

---

## `int ext4_listxattr(const char *path, char *list, size_t size, size_t *ret_size);`

List extended attributes.


- **`path`**: Path to file/directory.
- **`list`**: List to hold the name of entries.
- **`size`**: Size of @list in bytes.
- **`ret_size`**: Used bytes of @list.


- **Returns**: Standard error code.

---

## `int ext4_removexattr(const char *path, const char *name, size_t name_len);`

Remove extended attribute.


- **`path`**: Path to file/directory.
- **`name`**: Name of the entry to remove.
- **`name_len`**: Length of @name in bytes.


- **Returns**: Standard error code.

---

## `int ext4_dir_rm(const char *path);`

Recursive directory remove.


- **`path`**: Directory path to remove


- **Returns**: Standard error code.

---

## `int ext4_dir_mv(const char *path, const char *new_path);`

Rename/move directory.


- **`path`**: Source path.
- **`new_path`**: Destination path.


- **Returns**: Standard error code. 

---

## `int ext4_dir_mk(const char *path);`

Create new directory.


- **`path`**: Directory name.


- **Returns**: Standard error code.

---

## `int ext4_dir_open(ext4_dir *dir, const char *path);`

Directory open.


- **`dir`**: Directory handle.
- **`path`**: Directory path.


- **Returns**: Standard error code.

---

## `int ext4_dir_close(ext4_dir *dir);`

Directory close.


- **`dir`**: directory handle.


- **Returns**: Standard error code.

---

## `const ext4_direntry *ext4_dir_entry_next(ext4_dir *dir);`

Return next directory entry.


- **`dir`**: Directory handle.


- **Returns**: Directory entry id (NULL if no entry)

---

## `void ext4_dir_entry_rewind(ext4_dir *dir);`

Rewine directory entry offset.


- **`dir`**: Directory handle.

---

