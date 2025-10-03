#pragma once

#define MAX_NETDEV_NUM          8
#define MAX_CONNECTIONS         16
#define IFNAMSIZ                16
#define SCM_RIGHTS              0x01
#define SCM_CREDENTIALS         0x02c
#define MAX_SOCKETS             512
#define SOCKET_NAME_LEN         108
#define MAX_PENDING_FILES_COUNT 64
#define MAX_SOCKETS_NUM         16

#define SOL_SOCKET 1

#define SO_DEBUG        1
#define SO_REUSEADDR    2
#define SO_TYPE         3
#define SO_ERROR        4
#define SO_DONTROUTE    5
#define SO_BROADCAST    6
#define SO_SNDBUF       7
#define SO_RCVBUF       8
#define SO_SNDBUFFORCE  32
#define SO_RCVBUFFORCE  33
#define SO_KEEPALIVE    9
#define SO_OOBINLINE    10
#define SO_NO_CHECK     11
#define SO_PRIORITY     12
#define SO_LINGER       13
#define SO_BSDCOMPAT    14
#define SO_REUSEPORT    15
#define SO_PASSCRED     16
#define SO_PEERCRED     17
#define SO_RCVLOWAT     18
#define SO_SNDLOWAT     19
#define SO_RCVTIMEO_OLD 20
#define SO_SNDTIMEO_OLD 21

#define SO_SECURITY_AUTHENTICATION       22
#define SO_SECURITY_ENCRYPTION_TRANSPORT 23
#define SO_SECURITY_ENCRYPTION_NETWORK   24

#define SO_BINDTODEVICE 25

#define SO_ATTACH_FILTER 26
#define SO_DETACH_FILTER 27
#define SO_GET_FILTER    SO_ATTACH_FILTER

#define SO_PEERNAME 28

#define SO_ACCEPTCONN 30

#define SO_PEERSEC 31
#define SO_PASSSEC 34

#define SO_MARK 36

#define SO_PROTOCOL 38
#define SO_DOMAIN   39

#define SO_RXQ_OVFL 40

#define SO_WIFI_STATUS  41
#define SCM_WIFI_STATUS SO_WIFI_STATUS
#define SO_PEEK_OFF     42

#define SO_NOFCS 43

#define SO_LOCK_FILTER 44

#define SO_SELECT_ERR_QUEUE 45

#define SO_BUSY_POLL 46

#define SO_MAX_PACING_RATE 47

#define SO_BPF_EXTENSIONS 48

#define SO_INCOMING_CPU 49

#define SO_ATTACH_BPF 50
#define SO_DETACH_BPF SO_DETACH_FILTER

#define SO_ATTACH_REUSEPORT_CBPF 51
#define SO_ATTACH_REUSEPORT_EBPF 52

#define SO_CNX_ADVICE 53

#define SCM_TIMESTAMPING_OPT_STATS 54

#define SO_MEMINFO 55

#define SO_INCOMING_NAPI_ID 56

#define SO_COOKIE 57

#define SCM_TIMESTAMPING_PKTINFO 58

#define SO_PEERGROUPS 59

#define SO_ZEROCOPY 60

#define SO_TXTIME  61
#define SCM_TXTIME SO_TXTIME

#define SO_BINDTOIFINDEX 62

#define SO_TIMESTAMP_OLD    29
#define SO_TIMESTAMPNS_OLD  35
#define SO_TIMESTAMPING_OLD 37

#define SO_TIMESTAMP_NEW    63
#define SO_TIMESTAMPNS_NEW  64
#define SO_TIMESTAMPING_NEW 65

#define SO_RCVTIMEO_NEW 66
#define SO_SNDTIMEO_NEW 67

#define SO_DETACH_REUSEPORT_BPF 68

#define SO_PREFER_BUSY_POLL 69
#define SO_BUSY_POLL_BUDGET 70

#define SO_NETNS_COOKIE 71

#define SO_BUF_LOCK 72

#define SO_RESERVE_MEM 73

#define SO_TXREHASH 74

#define SO_RCVMARK 75

#define SO_PASSPIDFD 76
#define SO_PEERPIDFD 77

#define SO_DEVMEM_LINEAR   78
#define SCM_DEVMEM_LINEAR  SO_DEVMEM_LINEAR
#define SO_DEVMEM_DMABUF   79
#define SCM_DEVMEM_DMABUF  SO_DEVMEM_DMABUF
#define SO_DEVMEM_DONTNEED 80

#define BUFFER_SIZE 16 * 1024 * 1024

#define MSG_OOB          0x0001
#define MSG_PEEK         0x0002
#define MSG_DONTROUTE    0x0004
#define MSG_CTRUNC       0x0008
#define MSG_PROXY        0x0010
#define MSG_TRUNC        0x0020
#define MSG_DONTWAIT     0x0040
#define MSG_EOR          0x0080
#define MSG_WAITALL      0x0100
#define MSG_FIN          0x0200
#define MSG_SYN          0x0400
#define MSG_CONFIRM      0x0800
#define MSG_RST          0x1000
#define MSG_ERRQUEUE     0x2000
#define MSG_NOSIGNAL     0x4000
#define MSG_MORE         0x8000
#define MSG_WAITFORONE   0x10000
#define MSG_BATCH        0x40000
#define MSG_ZEROCOPY     0x4000000
#define MSG_FASTOPEN     0x20000000
#define MSG_CMSG_CLOEXEC 0x40000000

#define MAX_UEVENT_MESSAGES 32
#define MAX_NETLINK_SOCKETS 16
#define NETLINK_BUFFER_SIZE 32768

#define AF_NETLINK 16
#define PF_NETLINK AF_NETLINK

/* Netlink protocols */
#define NETLINK_ROUTE          0
#define NETLINK_UNUSED         1
#define NETLINK_USERSOCK       2
#define NETLINK_FIREWALL       3
#define NETLINK_SOCK_DIAG      4
#define NETLINK_NFLOG          5
#define NETLINK_XFRM           6
#define NETLINK_SELINUX        7
#define NETLINK_ISCSI          8
#define NETLINK_AUDIT          9
#define NETLINK_FIB_LOOKUP     10
#define NETLINK_CONNECTOR      11
#define NETLINK_NETFILTER      12
#define NETLINK_IP6_FW         13
#define NETLINK_DNRTMSG        14
#define NETLINK_KOBJECT_UEVENT 15
#define NETLINK_GENERIC        16
#define NETLINK_SCSITRANSPORT  18
#define NETLINK_ECRYPTFS       19
#define NETLINK_RDMA           20
#define NETLINK_CRYPTO         21
#define NETLINK_SMC            22

/* Netlink message flags */
#define NLM_F_REQUEST       1
#define NLM_F_MULTI         2
#define NLM_F_ACK           4
#define NLM_F_ECHO          8
#define NLM_F_DUMP_INTR     16
#define NLM_F_DUMP_FILTERED 32

#define NLM_F_ROOT   0x100
#define NLM_F_MATCH  0x200
#define NLM_F_ATOMIC 0x400
#define NLM_F_DUMP   (NLM_F_ROOT | NLM_F_MATCH)

#define NLM_F_REPLACE 0x100
#define NLM_F_EXCL    0x200
#define NLM_F_CREATE  0x400
#define NLM_F_APPEND  0x800

/* Netlink message types */
#define NLMSG_NOOP     0x1
#define NLMSG_ERROR    0x2
#define NLMSG_DONE     0x3
#define NLMSG_OVERRUN  0x4
#define NLMSG_MIN_TYPE 0x10

#define __CMSG_LEN(cmsg)  (((cmsg)->cmsg_len + sizeof(long) - 1) & ~(long)(sizeof(long) - 1))
#define __CMSG_NEXT(cmsg) ((unsigned char *)(cmsg) + __CMSG_LEN(cmsg))
#define __MHDR_END(mhdr)  ((unsigned char *)(mhdr)->msg_control + (mhdr)->msg_controllen)

#define CMSG_DATA(cmsg) ((unsigned char *)(((struct cmsghdr *)(cmsg)) + 1))
#define CMSG_NXTHDR(mhdr, cmsg)                                                                    \
    ((cmsg)->cmsg_len < sizeof(struct cmsghdr) || __CMSG_LEN(cmsg) + sizeof(struct cmsghdr) >=     \
                                                      __MHDR_END(mhdr) - (unsigned char *)(cmsg)   \
         ? 0                                                                                       \
         : (struct cmsghdr *)__CMSG_NEXT(cmsg))
#define CMSG_FIRSTHDR(mhdr)                                                                        \
    ((size_t)(mhdr)->msg_controllen >= sizeof(struct cmsghdr)                                      \
         ? (struct cmsghdr *)(mhdr)->msg_control                                                   \
         : (struct cmsghdr *)0)

#define CMSG_ALIGN(len) (((len) + sizeof(size_t) - 1) & (size_t)~(sizeof(size_t) - 1))
#define CMSG_SPACE(len) (CMSG_ALIGN(len) + CMSG_ALIGN(sizeof(struct cmsghdr)))
#define CMSG_LEN(len)   (CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))

/* Netlink message length macros */
#define NLMSG_ALIGN(len)  (((len) + 3) & ~3)
#define NLMSG_LENGTH(len) ((len) + sizeof(struct nlmsghdr))
#define NLMSG_SPACE(len)  NLMSG_ALIGN(NLMSG_LENGTH(len))
#define NLMSG_DATA(nlh)   ((void *)((char *)(nlh) + NLMSG_LENGTH(0)))
#define NLMSG_NEXT(nlh, len)                                                                       \
    ((len) -= NLMSG_ALIGN((nlh)->nlmsg_len),                                                       \
     (struct nlmsghdr *)((char *)(nlh) + NLMSG_ALIGN((nlh)->nlmsg_len)))
#define NLMSG_OK(nlh, len)                                                                         \
    ((len) >= (int)sizeof(struct nlmsghdr) && (nlh)->nlmsg_len >= sizeof(struct nlmsghdr) &&       \
     (nlh)->nlmsg_len <= (len))

#include "ctype.h"
#include "lock.h"
#include "syscall.h"
#include "vfs.h"

typedef errno_t (*netdev_send_t)(void *dev, void *data, uint32_t len);
typedef errno_t (*netdev_recv_t)(void *dev, void *data, uint32_t len);
typedef uint32_t socklen_t;

typedef enum {
    SOCKET_TYPE_UNUSED,
    SOCKET_TYPE_UNCONNECTED,
    SOCKET_TYPE_LISTENING,
    SOCKET_TYPE_CONNECTED
} socket_state_t;

typedef struct netdev {
    uint8_t       mac[6];
    uint32_t      mtu;
    void         *desc;
    netdev_send_t send;
    netdev_recv_t recv;
} netdev_t;

struct msghdr {
    void         *msg_name;       /* ptr to socket address structure */
    int           msg_namelen;    /* size of socket address structure */
    struct iovec *msg_iov;        /* scatter/gather array */
    size_t        msg_iovlen;     /* # elements in msg_iov */
    void         *msg_control;    /* ancillary data */
    size_t        msg_controllen; /* ancillary data buffer length */
    uint32_t      msg_flags;      /* flags on received message */
};

struct sockaddr_un {
    uint16_t sun_family;
    char     sun_path[SOCKET_NAME_LEN];
};

struct linger {
    int l_onoff;  // linger active
    int l_linger; // linger time (seconds)
};

struct sock_filter {
    uint16_t code;
    uint8_t  jt;
    uint8_t  jf;
    uint32_t k;
};

struct sock_fprog {
    uint16_t            len;
    struct sock_filter *filter;
};

struct ucred {
    int32_t  pid;
    uint32_t uid;
    uint32_t gid;
};

struct sockaddr_nl {
    uint16_t       nl_family;
    unsigned short nl_pad;
    uint32_t       nl_pid;
    uint32_t       nl_groups;
};

typedef struct unix_socket_pair {
    spin_t lock;

    // accept()/server
    bool     established;
    int      serverFds;
    uint8_t *serverBuff;
    size_t   serverBuffPos;
    size_t   serverBuffSize;

    struct ucred server;
    struct ucred client;

    char *filename;

    // connect()/client
    int      clientFds;
    uint8_t *clientBuff;
    int      clientBuffPos;
    int      clientBuffSize;

    // msg_control/msg_controllen
    fd_file_handle **client_pending_files;
    fd_file_handle **server_pending_files;
    int              pending_fds_size;

    int                 reuseaddr;
    int                 keepalive;
    struct timeval      sndtimeo;
    struct timeval      rcvtimeo;
    char                bind_to_dev[IFNAMSIZ];
    struct linger       linger_opt;
    int                 passcred;
    struct sock_filter *filter;
    size_t              filter_len;
    struct ucred        peercred;
    bool                has_peercred;
} unix_socket_pair_t;

typedef struct socket {
    struct socket       *next;
    int                  domain;
    int                  protocol;
    int                  timesOpened;
    // accept()
    bool                 acceptWouldBlock;
    // bind()
    char                *bindAddr;
    // listen()
    int                  connMax; // if 0, listen() hasn't ran
    int                  connCurr;
    unix_socket_pair_t **backlog;
    int                  passcred;
    // connect()
    unix_socket_pair_t  *pair;
} socket_t;

typedef struct socket_op {
    uint64_t (*shutdown)(uint64_t fd, uint64_t how);
    size_t (*getpeername)(uint64_t fd, struct sockaddr_un *addr, socklen_t *addrlen);
    int (*getsockname)(uint64_t fd, struct sockaddr_un *addr, socklen_t *addrlen);
    int (*bind)(uint64_t fd, const struct sockaddr_un *addr, socklen_t addrlen);
    int (*listen)(uint64_t fd, int backlog);
    int (*accept)(uint64_t fd, struct sockaddr_un *addr, socklen_t *addrlen, uint64_t flags);
    int (*connect)(uint64_t fd, const struct sockaddr_un *addr, socklen_t addrlen);
    size_t (*sendto)(uint64_t fd, uint8_t *in, size_t limit, int flags, struct sockaddr_un *addr,
                     uint32_t len);
    size_t (*recvfrom)(uint64_t fd, uint8_t *out, size_t limit, int flags, struct sockaddr_un *addr,
                       uint32_t *len);
    size_t (*recvmsg)(uint64_t fd, struct msghdr *msg, int flags);
    size_t (*sendmsg)(uint64_t fd, const struct msghdr *msg, int flags);
    size_t (*getsockopt)(uint64_t fd, int level, int optname, const void *optval,
                         socklen_t *optlen);
    size_t (*setsockopt)(uint64_t fd, int level, int optname, const void *optval, socklen_t optlen);
} socket_op_t;

typedef struct socket_handle {
    fd_file_handle *fd;
    void           *sock;
    socket_op_t    *op;
} socket_handle_t;

struct cmsghdr {
    uint64_t cmsg_len;
    int      cmsg_level;
    int      cmsg_type;
};

struct uevent_message {
    char     buffer[NETLINK_BUFFER_SIZE];
    size_t   length;
    uint64_t timestamp;
};

struct netlink_sock {
    uint32_t            portid;
    uint32_t            groups;
    uint32_t            dst_portid;
    uint32_t            dst_groups;
    struct sockaddr_nl *bind_addr;
    int                 uevent_message_pos;
    char               *buffer;
    size_t              buffer_size;
    size_t              buffer_pos;
    spin_t              lock;
};

typedef struct real_socket_socket {
    int domain;
    int (*socket)(int domain, int type, int protocol);
} real_socket_socket_t;

extern int                   unix_socket_fsid;
extern int                   unix_accept_fsid;
extern vfs_node_t            sockfs_root;
extern socket_t              first_unix_socket;
extern socket_op_t           socket_ops;
extern socket_op_t           accept_ops;
extern int                   sockfsfd_id;
extern real_socket_socket_t *real_sockets[];
extern int                   socket_num;

void regist_netdev(void *desc, uint8_t *mac, uint32_t mtu, netdev_send_t send, netdev_recv_t recv);
errno_t netdev_send(netdev_t *dev, void *data, uint32_t len);
errno_t netdev_recv(netdev_t *dev, void *data, uint32_t len);
void    netfs_setup();

int     socket_socket(int domain, int type, int protocol);
int     socket_bind(uint64_t fd, const struct sockaddr_un *addr, socklen_t addrlen);
void    socket_socket_close(void *handle);
void    socket_accept_close(void *handle0);
errno_t socket_socket_poll(void *file, size_t events);
errno_t socket_accept_poll(void *file, size_t events);
void    unix_socket_free_pair(unix_socket_pair_t *pair);
void    regist_socket(int domain, int (*socket)(int domain, int type, int protocol));

int  netlink_socket(int domain, int type, int protocol);
void netlink_init();

errno_t  syscall_net_socket(int domain, int type, int protocol);
uint64_t syscall_net_bind(int sockfd, const struct sockaddr_un *addr, socklen_t addrlen);
uint64_t syscall_net_listen(int sockfd, int backlog);
uint64_t syscall_net_connect(int sockfd, const struct sockaddr_un *addr, socklen_t addrlen);
int64_t  syscall_net_sendmsg(int sockfd, const struct msghdr *msg, int flags);
int64_t  syscall_net_recvmsg(int sockfd, struct msghdr *msg, int flags);
uint64_t syscall_net_accept(int sockfd, struct sockaddr_un *addr, socklen_t *addrlen,
                            uint64_t flags);
int64_t  syscall_net_recv(int sockfd, void *buf, size_t len, int flags,
                          struct sockaddr_un *dest_addr, socklen_t *addrlen);
int64_t  syscall_net_send(int sockfd, void *buff, size_t len, int flags,
                          struct sockaddr_un *dest_addr, socklen_t addrlen);
