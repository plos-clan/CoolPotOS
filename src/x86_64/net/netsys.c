#include "network.h"
#include "syscall.h"

errno_t syscall_net_socket(int domain, int type, int protocol) {
    errno_t fd = -EAFNOSUPPORT;

    return -ENOSYS;
}
