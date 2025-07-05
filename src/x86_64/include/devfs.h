#pragma once

#include "ctype.h"

void   devfs_setup();
void   devfs_regist_dev(int drive_id);
size_t devfs_read(void *file, void *addr, size_t offset, size_t size);
size_t devfs_write(void *file, const void *addr, size_t offset, size_t size);
