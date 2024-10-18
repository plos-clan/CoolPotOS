#ifndef CRASHPOWERDOS_DEVFS_H
#define CRASHPOWERDOS_DEVFS_H

#include "vfs.h"
#include "common.h"

typedef struct node_t{
    char* name;               // 节点名
    int   n_children;         // 子节点个数
    int   level;              // 记录该节点在多叉树中的层数
    struct node_t** children; // 指向其自身的子节点，children一个数组，该数组中的元素时node_t*指针
} NODE;

void register_devfs();

#endif
