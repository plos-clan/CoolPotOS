#pragma once

#include "cp_kernel.h"
#include "errno.h"
#include "fs_subsystem.h"
#include "sqfs/compressor.h"
#include "sqfs/data_reader.h"
#include "sqfs/dir.h"
#include "sqfs/dir_reader.h"
#include "sqfs/id_table.h"
#include "sqfs/inode.h"
#include "sqfs/io.h"
#include "sqfs/super.h"
#include "sqfs/error.h"

typedef struct squashfs_mount squashfs_mount_t;
typedef struct squashfs_handle squashfs_handle_t;

struct squashfs_mount {
    size_t              refcount;
    sqfs_file_t        *image;
    sqfs_compressor_t  *cmp;
    sqfs_id_table_t    *ids;
    sqfs_dir_reader_t  *dir_reader;
    sqfs_data_reader_t *data_reader;
    sqfs_super_t        super;
};

struct squashfs_handle {
    squashfs_mount_t      *mount;
    sqfs_inode_generic_t  *inode;
    sqfs_u64               inode_ref;
};

int  squashfs_create_mount(const char *src, squashfs_mount_t **out, sqfs_inode_generic_t **root_inode, sqfs_u64 *root_ref);
void squashfs_mount_grab(squashfs_mount_t *mount);
void squashfs_mount_drop(squashfs_mount_t *mount);
int  squashfs_open_inode(squashfs_mount_t *mount, sqfs_u64 inode_ref, sqfs_inode_generic_t **out);
int  squashfs_lookup_child(squashfs_mount_t *mount, const sqfs_inode_generic_t *parent, const char *name, sqfs_u64 *inode_ref, sqfs_inode_generic_t **out);
void squashfs_fill_node(vfs_node_t node, squashfs_handle_t *handle);
int  squashfs_populate_dir(vfs_node_t node, const squashfs_handle_t *handle);
int  squashfs_map_inode_type(sqfs_u16 type);
