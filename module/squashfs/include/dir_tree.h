/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dir_tree.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef DIR_TREE_H
#define DIR_TREE_H

/**
 * @enum SQFS_TREE_FILTER_FLAGS
 *
 * @brief Filter flags for @ref sqfs_dir_reader_get_full_hierarchy
 */
typedef enum {
	/**
	 * @brief Omit device special files from the final tree.
	 */
	SQFS_TREE_NO_DEVICES = 0x01,

	/**
	 * @brief Omit socket files from the final tree.
	 */
	SQFS_TREE_NO_SOCKETS = 0x02,

	/**
	 * @brief Omit named pipes from the final tree.
	 */
	SQFS_TREE_NO_FIFO = 0x04,

	/**
	 * @brief Omit symbolic links from the final tree.
	 */
	SQFS_TREE_NO_SLINKS = 0x08,

	/**
	 * @brief Omit empty directories from the final tree.
	 *
	 * If a directory is not empty on-disk, but ends up empty after
	 * applying all the other filter rules, it is also omitted.
	 */
	SQFS_TREE_NO_EMPTY = 0x10,

	/**
	 * @brief Do not recurse into sub directories.
	 *
	 * If the start node is a directory, the tree deserializer will still
	 * recurse into it, but it will not go beyond that.
	 */
	SQFS_TREE_NO_RECURSE = 0x20,

	/**
	 * @brief Store the list of parent nodes all the way to the target node
	 *
	 * When traversing towards the selected node, also collect the chain
	 * of parent nodes with the subtree stored at the end.
	 */
	SQFS_TREE_STORE_PARENTS = 0x40,

	SQFS_TREE_ALL_FLAGS = 0x7F,
} SQFS_TREE_FILTER_FLAGS;

/**
 * @struct sqfs_tree_node_t
 *
 * @brief Encapsulates a node in the filesystem tree read by
 *        @ref sqfs_dir_reader_get_full_hierarchy.
 */
struct sqfs_tree_node_t {
	/**
	 * @brief Pointer to parent, NULL for the root node
	 */
	sqfs_tree_node_t *parent;

	/**
	 * @brief For directories, a linked list of children.
	 */
	sqfs_tree_node_t *children;

	/**
	 * @brief Linked list next pointer for children list.
	 */
	sqfs_tree_node_t *next;

	/**
	 * @brief Inode representing this element in the tree.
	 */
	sqfs_inode_generic_t *inode;

	/**
	 * @brief Resolved 32 bit user ID from the inode
	 */
	sqfs_u32 uid;

	/**
	 * @brief Resolved 32 bit group ID from the inode
	 */
	sqfs_u32 gid;

	/**
	 * @brief null-terminated entry name.
	 */
	sqfs_u8 name[];
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Recursively destroy a tree of @ref sqfs_tree_node_t nodes
 *
 * This function can be used to clean up after
 * @ref sqfs_dir_reader_get_full_hierarchy.
 *
 * @param root A pointer to the root node or NULL.
 */
SQFS_INTERNAL void sqfs_dir_tree_destroy(sqfs_tree_node_t *root);

/**
 * @brief Recursively destroy a tree of @ref sqfs_tree_node_t nodes
 *
 * @memberof sqfs_tree_node_t
 *
 * This function can be used to assemble an absolute path from a tree
 * node returned by @ref sqfs_dir_reader_get_full_hierarchy.
 *
 * The function recursively walks up the tree to assemble a path string. It
 * returns "/" for the root node and assembles paths beginning with "/" for
 * non-root nodes. The resulting path is slash separated, but (except for
 * the root) never ends with a slash.
 *
 * While walking the node list, the function enforces various invariantes. It
 * returns @ref SQFS_ERROR_LINK_LOOP if the list of parent pointers is cyclical,
 * @ref SQFS_ERROR_CORRUPTED if any node has an empty name, or a name that
 * contains '/' or equals ".." or ".". The function
 * returns @ref SQFS_ERROR_ARG_INVALID if given NULL node or the root has a name
 * set. Additionally, the function can return overflow or allocation failures
 * while constructing the path.
 *
 * The returned string needs to be free'd with @ref sqfs_free.
 *
 * @param node A pointer to a tree node.
 * @param out Returns a pointer to a string on success, set to NULL on failure.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_INTERNAL int sqfs_tree_node_get_path(const sqfs_tree_node_t *node,
					  char **out);

/**
 * @brief High level helper function for deserializing the entire file system
 *        hierarchy into an in-memory tree structure.
 *
 * @memberof sqfs_dir_reader_t
 *
 * This function internally navigates to a specified inode using
 * @ref sqfs_dir_reader_find_by_path and starting from that recursively
 * deserializes the entire hierarchy into a tree structure holding all inodes.
 *
 * @param rd A pointer to a directory reader.
 * @param path A path to resolve into an inode. Forward or backward slashes can
 *             be used to separate path components. Resolving '.' or '..' is
 *             not supported. Can be set to NULL to get the root inode.
 * @param flags A combination of @ref SQFS_TREE_FILTER_FLAGS flags.
 * @param out Returns the top most tree node.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_INTERNAL int sqfs_dir_reader_get_full_hierarchy(sqfs_dir_reader_t *rd,
						const sqfs_id_table_t *idtbl,
						const char *path,
						sqfs_u32 flags,
						sqfs_tree_node_t **out);

#ifdef __cplusplus
}
#endif

#endif /* DIR_TREE_H */
