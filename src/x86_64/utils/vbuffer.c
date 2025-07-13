#include "vbuffer.h"
#include "heap.h"

struct vecbuf *vbuf_alloc(struct vecbuf **vec, void *buf, size_t size) {
    struct vecbuf *vbuf = malloc(sizeof(struct vecbuf));
    struct vecbuf *_vec = *vec;

    *vbuf = (struct vecbuf){
        .buf = {.buffer = buf, .size = size},
          .acc_sz = vbuf_size(_vec) + size
    };

    if (_vec) {
        llist_append(&_vec->components, &vbuf->components);
    } else {
        llist_init_head(&vbuf->components);
        *vec = vbuf;
    }

    return vbuf;
}

void vbuf_free(struct vecbuf *vbuf) {
    struct vecbuf *pos, *n;
    llist_for_each(pos, n, &vbuf->components, components) {
        free(pos);
    }
    free(vbuf);
}

size_t vbuf_size(struct vecbuf *vbuf) {
    if (!vbuf) { return 0; }

    struct vecbuf *last = list_entry(vbuf->components.prev, struct vecbuf, components);
    return last->acc_sz;
}

void vbuf_chunkify(struct vecbuf **vbuf, void *buffer, size_t total_size, size_t chunk_size) {
    size_t offset = 0;
    while (offset < total_size) {
        size_t remain     = total_size - offset;
        size_t this_chunk = remain > chunk_size ? chunk_size : remain;
        vbuf_alloc(vbuf, (uint8_t *)buffer + offset, this_chunk);
        offset += this_chunk;
    }
}
