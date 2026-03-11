/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FSTREE_H
#define FSTREE_H

#include "config.h"

#include "cp_kernel.h"

#include "sqfs/predef.h"
#include "sqfs/dir_entry.h"
#include "compat.h"

typedef struct fstree_defaults_t fstree_defaults_t;
typedef struct tree_node_t tree_node_t;
typedef struct fstree_t fstree_t;
typedef struct fstree_stats_t fstree_stats_t;

enum {
	FLAG_DIR_CREATED_IMPLICITLY = 0x01,
	FLAG_FILE_ALREADY_MATCHED = 0x02,
	FLAG_LINK_IS_HARD = 0x04,
	FLAG_LINK_RESOVED = 0x08,
};

/* A node in a file system tree */
struct tree_node_t {
	tree_node_t *next_by_type;

	/* Parent directory children linked list pointer. */
	tree_node_t *next;

	/* Root node has this set to NULL. */
	tree_node_t *parent;

	/* For the root node, this points to an empty string. */
	char *name;

	sqfs_u32 xattr_idx;
	sqfs_u32 uid;
	sqfs_u32 gid;
	sqfs_u32 inode_num;
	sqfs_u32 mod_time;
	sqfs_u32 link_count;
	sqfs_u16 mode;
	sqfs_u16 flags;

	/* SquashFS inode refernce number. 32 bit offset of the meta data
	   block start (relative to inode table start), shifted left by 16
	   and ored with a 13 bit offset into the uncompressed meta data block.

	   Generated on the fly when writing inodes. */
	sqfs_u64 inode_ref;

	/* Type specific data. "target" pointer is into payload area below. */
	union {
		struct {
			/* Path to the input file. */
			char *input_file;

			sqfs_inode_generic_t *inode;

			/* used by sort file processing */
			sqfs_s64 priority;
			int flags;
		} file;

		tree_node_t *children;
		char *target;
		sqfs_u64 devno;
		tree_node_t *target_node;
	} data;

	sqfs_u8 payload[];
};

struct fstree_stats_t {
	size_t num_ipc;
	size_t num_links;
	size_t num_slinks;
	size_t num_files;
	size_t num_devices;
	size_t num_dirs;
};

/* Default settings for new nodes */
struct fstree_defaults_t {
	sqfs_u32 uid;
	sqfs_u32 gid;
	sqfs_u32 mtime;
	sqfs_u16 mode;
};

/* Encapsulates a file system tree */
struct fstree_t {
	fstree_defaults_t defaults;

	size_t unique_inode_count;

	/* flat array of all nodes that have an inode number */
	tree_node_t **inodes;

	tree_node_t *root;

	/* linear linked list of all regular files */
	tree_node_t *files;

	/* linear linked list of all unresolved hard links */
	tree_node_t *links_unresolved;
};

/*
  Initializing means copying over the default values and creating a root node.
  On error, an error message is written to stderr.

  Returns 0 on success.
*/
int fstree_init(fstree_t *fs, const fstree_defaults_t *defaults);

void fstree_cleanup(fstree_t *fs);

/*
  Add a node to an fstree at a specific path.

  If some components of the path don't exist, they are created as directories
  with default permissions, like mkdir -p would, and marked as implcitily
  created. A subsequent call that tries to create an existing tree node will
  fail, except if the target is an implicitly created directory node and the
  call tries to create it as a directory (this will simply overwrite the
  permissions and ownership). The implicitly created flag is then cleared.
  Subsequent attempts to create an existing directory again will then also
  fail.

  This function does not print anything to stderr, instead it sets an
  appropriate errno value. Internally it uses fstree_mknode to create the node.
*/
tree_node_t *fstree_add_generic(fstree_t *fs, const sqfs_dir_entry_t *ent,
				const char *extra);

/*
  This function performs all the necessary post processing steps on the file
  system tree, i.e. recursively sorting all directory entries by name,
  allocating inode numbers, resolving hard links and stringing all files nodes
  together into a linked list.

  The total inode count is stored in unique_inode_count. The head of the file
  list is pointed to by fs->files.

  The "inodes" array is allocated and each node that has an inode number is
  mapped into the array at index inode_num - 1.

  Returns 0 on success, prints to stderr on failure.
 */
int fstree_post_process(fstree_t *fs);

/*
  Generate a string holding the full path of a node. Returned
  string must be freed.

  Returns NULL on failure and sets errno.
*/
char *fstree_get_path(tree_node_t *node);

/*
  Resolve a path to a tree node. Returns NULL on failure and sets errno.

  If "create_implicitly" is set to true, the function acts essentially like
  mkdir_p if part of the path doesn't exist yet.

  If "stop_at_parent" is true, the function stops at the last component and
  returns the parent or would-be-parent of the last path component, but doesn't
  check if the last path component exists or not.
 */
tree_node_t *fstree_get_node_by_path(fstree_t *fs, tree_node_t *root,
				     const char *path, bool create_implicitly,
				     bool stop_at_parent);

/*
  Try to resolve all hard links in the tree.

  Returns 0 on success. On failure, errno is set.
 */
int fstree_resolve_hard_links(fstree_t *fs);

void fstree_collect_stats(const fstree_t *fs, fstree_stats_t *out);

#endif /* FSTREE_H */
