#pragma once

#include "ctype.h"
#include "llist.h"

struct membuf {
    void  *buffer;
    size_t size;
};

struct vecbuf {
    struct llist_header components;
    struct membuf       buf;
    size_t              acc_sz;
};

void           vbuf_free(struct vecbuf *vbuf);
struct vecbuf *vbuf_alloc(struct vecbuf **vec, void *buf, size_t len);
void           vbuf_from_vaddr(struct vecbuf **out_vbuf, void *vaddr, size_t size);
void   vbuf_chunkify(struct vecbuf **vbuf, void *buffer, size_t total_size, size_t chunk_size);
size_t vbuf_size(struct vecbuf *vbuf);
