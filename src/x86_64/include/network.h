#pragma once

#include "ctype.h"

errno_t syscall_net_socket(int domain, int type, int protocol);
void    netfs_setup();
