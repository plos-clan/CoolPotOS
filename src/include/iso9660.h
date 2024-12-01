#pragma once

#define L9660_SEEK_END -1
#define L9660_SEEK_SET 0
#define L9660_SEEK_CUR +1

#include "ctypes.h"
#include "vfs.h"

typedef enum {
    L9660_OK = 0,
    L9660_EIO,
    L9660_EBADFS,
    L9660_ENOENT,
    L9660_ENOTFILE,
    L9660_ENOTDIR,
} l9660_status;

typedef struct {
    uint8_t le[2];
} l9660_luint16;
typedef struct {
    uint8_t be[2];
} l9660_buint16;
typedef struct {
    uint8_t le[2], be[2];
} l9660_duint16;
typedef struct {
    uint8_t le[4];
} l9660_luint32;
typedef struct {
    uint8_t be[4];
} l9660_buint32;
typedef struct {
    uint8_t le[4], be[4];
} l9660_duint32;

/* Descriptor time format */
typedef struct {
    char d[17];
} l9660_desctime;

/* File time format */
typedef struct {
    char d[7];
} l9660_filetime;

/* Directory entry */
typedef struct {
    uint8_t        length;
    uint8_t        xattr_length;
    l9660_duint32  sector;
    l9660_duint32  size;
    l9660_filetime time;
    uint8_t        flags;
    uint8_t        unit_size;
    uint8_t        gap_size;
    l9660_duint16  vol_seq_number;
    uint8_t        name_len;
    char           name[/*name_len*/];
} l9660_dirent;

/* Volume descriptor header */
typedef struct {
    uint8_t type;
    char    magic[5];
    uint8_t version;
} l9660_vdesc_header;

/* Primary volume descriptor */
typedef struct {
    l9660_vdesc_header hdr;
    char               pad0[1];
    char               system_id[32];
    char               volume_id[32];
    char               pad1[8];
    l9660_duint32      volume_space_size;
    char               pad2[32];
    l9660_duint16      volume_set_size;
    l9660_duint16      volume_seq_number;
    l9660_duint16      logical_block_size;
    l9660_duint32      path_table_size;
    l9660_luint32      path_table_le;
    l9660_luint32      path_table_opt_le;
    l9660_buint32      path_table_be;
    l9660_buint32      path_table_opt_be;
    union {
        l9660_dirent root_dir_ent;
        char         pad3[34];
    };
    char           volume_set_id[128];
    char           data_preparer_id[128];
    char           app_id[128];
    char           copyright_file[38];
    char           abstract_file[36];
    char           bibliography_file[37];
    l9660_desctime volume_created, volume_modified, volume_expires, volume_effective;
    uint8_t        file_structure_version;
    char           pad4[1];
    char           app_reserved[512];
    char           reserved[653];
} l9660_vdesc_primary;

/* A generic volume descriptor (i.e. 2048 bytes) */
typedef union {
    l9660_vdesc_header hdr;
    char               _bits[2048];
} l9660_vdesc;

typedef struct l9660_file;

typedef struct l9660_fs {
#ifdef L9660_SINGLEBUFFER
    union {
    l9660_dirent root_dir_ent;
    char         root_dir_pad[34];
  };
#else
    /* Sector buffer to hold the PVD */
    l9660_vdesc pvd;
#endif

    /* read_sector func */
    bool (*read_sector)(struct l9660_fs *fs, void *buf, uint32_t sector);
    vfs_node_t device;
} l9660_fs;

typedef struct {
#ifndef L9660_SINGLEBUFFER
    /* single sector buffer */
    char buf[2048];
#endif
    l9660_fs *fs;
    uint32_t  first_sector;
    uint32_t  position;
    uint32_t  length;
} l9660_file;

typedef struct {
    /* directories are mostly just files with special accessors, but we like type safetey */
    l9660_file file;
} l9660_dir;

typedef struct l9660_fs_status {
    l9660_fs *fs;
    l9660_dir root_dir;
    l9660_dir now_dir;

} l9660_fs_status_t;

typedef struct file {
    int   type;
    void *handle; // file or dir handle
} *file_t;

uint32_t     l9660_tell(l9660_file *f);
l9660_status l9660_read(l9660_file *f, void *buf, size_t size, size_t *read);
l9660_status l9660_seek(l9660_file *f, int whence, int32_t offset);
l9660_status l9660_openat(l9660_file *child, l9660_dir *parent, const char *name);
l9660_status l9660_readdir(l9660_dir *dir, l9660_dirent **pdirent);
l9660_status l9660_opendirat(l9660_dir *dir, l9660_dir *parent, const char *path);
l9660_status l9660_fs_open_root(l9660_dir *dir, l9660_fs *fs);
l9660_status l9660_openfs(l9660_fs *fs,
                          bool (*read_sector)(l9660_fs *fs, void *buf, uint32_t sector),
                          vfs_node_t device);

bool read_sector(l9660_fs *fs, void *buf, uint32_t sector);
void iso9660_regist();

