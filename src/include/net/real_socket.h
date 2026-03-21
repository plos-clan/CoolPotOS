#pragma once

#include "net/socket.h"

typedef struct real_socket_socket {
    int domain;
    int (*init)(void);
    int (*socket)(int domain, int type, int protocol);
    int (*socketpair)(int family, int type, int protocol, int *sv);
} real_socket_socket_t;

#define MAX_SOCKETS_NUM 16

void regist_socket(
    int domain,
    int (*init)(void),
    int (*socket)(int domain, int type, int protocol),
    int (*socketpair)(int family, int type, int protocol, int *sv)
);

extern real_socket_socket_t *real_sockets[MAX_SOCKETS_NUM];
extern int socket_num;

void real_socket_init(void);
