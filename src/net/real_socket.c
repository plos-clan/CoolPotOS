#include "net/real_socket.h"
#include "mem/alloc.h"

real_socket_socket_t *real_sockets[MAX_SOCKETS_NUM] = {NULL};
static spin_t real_sockets_lock = SPIN_INIT;
int socket_num = 0;
static bool real_socket_initialized = false;

void regist_socket(int domain, int (*init)(),
                   int (*socket)(int domain, int type, int protocol),
                   int (*socketpair)(int family, int type, int protocol,
                                     int *sv)) {
    spin_lock(real_sockets_lock);
    real_sockets[socket_num] = malloc(sizeof(real_socket_socket_t));
    real_sockets[socket_num]->domain = domain;
    real_sockets[socket_num]->init = init;
    real_sockets[socket_num]->socket = socket;
    real_sockets[socket_num]->socketpair = socketpair;
    socket_num++;
    spin_unlock(real_sockets_lock);
}

void real_socket_init() {
    if (real_socket_initialized)
        return;
    real_socket_initialized = true;

    for (int i = 0; i < socket_num; i++) {
        if (real_sockets[i] && real_sockets[i]->init) {
            real_sockets[i]->init();
        }
    }
}
