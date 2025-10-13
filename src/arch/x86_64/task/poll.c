#include "poll.h"
#include "heap.h"

uint32_t epoll_to_poll_comp(uint32_t epoll_events) {
    uint32_t poll_events = 0;

    if (epoll_events & EPOLLIN) { poll_events |= POLLIN; }
    if (epoll_events & EPOLLOUT) { poll_events |= POLLOUT; }
    if (epoll_events & EPOLLPRI) { poll_events |= POLLPRI; }
    if (epoll_events & EPOLLERR) { poll_events |= POLLERR; }
    if (epoll_events & EPOLLHUP) { poll_events |= POLLHUP; }

    return poll_events;
}

uint32_t poll_to_epoll_comp(uint32_t poll_events) {
    uint32_t epoll_events = 0;

    if (poll_events & POLLIN) { epoll_events |= EPOLLIN; }
    if (poll_events & POLLOUT) { epoll_events |= EPOLLOUT; }
    if (poll_events & POLLPRI) { epoll_events |= EPOLLPRI; }
    if (poll_events & POLLERR) { epoll_events |= EPOLLERR; }
    if (poll_events & POLLHUP) { epoll_events |= EPOLLHUP; }

    return epoll_events;
}

struct pollfd *select_add(struct pollfd **comp, size_t *compIndex, size_t *complength, int fd,
                          int events) {
    if ((*compIndex + 1) * sizeof(struct pollfd) >= *complength) {
        *complength *= 2;
        *comp        = realloc(*comp, *complength);
    }

    (*comp)[*compIndex].fd      = fd;
    (*comp)[*compIndex].events  = events;
    (*comp)[*compIndex].revents = 0;

    return &(*comp)[(*compIndex)++];
}

bool select_bitmap(const uint8_t *map, int index) {
    int div = index / 8;
    int mod = index % 8;
    return map[div] & (1 << mod);
}

void select_bitmap_set(uint8_t *map, int index) {
    int div   = index / 8;
    int mod   = index % 8;
    map[div] |= 1 << mod;
}
