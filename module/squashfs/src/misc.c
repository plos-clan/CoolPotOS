/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * misc.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/predef.h"

#include <stdlib.h>

void sqfs_free(void *ptr)
{
	free(ptr);
}
