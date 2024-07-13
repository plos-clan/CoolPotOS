#include "../include/heap.h"
#include "../include/printf.h"

extern struct task_struct *current;

static header_t *use_get_free_block(struct task_struct *user_t, size_t size) {
    header_t *curr = user_t->head;
    while (curr) {
        if (curr->s.is_free && curr->s.size >= size) return curr;
        curr = curr->s.next;
    }
    return NULL;
}

void *use_sbrk(struct task_struct *user_t,int incr) {
    if (user_t->program_break + incr >= (user_t->program_break_end)) return (void *) -1;
    void *prev_break = user_t->program_break;
    user_t->program_break += incr;
    return prev_break;
}

void *user_alloc(struct task_struct *user_t, size_t size) {
    uint32_t total_size;
    void *block;
    header_t *header;
    if (!size) return NULL;
    header = use_get_free_block(user_t,size);
    if (header) {
        header->s.is_free = 0;
        return (void *) (header + 1);
    }
    total_size = sizeof(header_t) + size;
    block = use_sbrk(user_t,total_size);
    if (block == (void *) -1) return NULL;
    header = block;
    header->s.size = size;
    header->s.is_free = 0;
    header->s.next = NULL;
    if (!user_t->head) user_t->head = header;
    if (user_t->tail) user_t->tail->s.next = header;
    user_t->tail = header;
    return (void *) (header + 1);
}

void use_free(struct task_struct *user_t,void *block) {
    header_t *header, *tmp;
    if (!block) return;
    header = (header_t *) block - 1;
    if ((char *) block + header->s.size == user_t->program_break) {
        if (user_t->head == user_t->tail) user_t->head = user_t->tail = NULL;
        else {
            tmp = user_t->head;
            while (tmp) {
                if (tmp->s.next == user_t->tail) {
                    tmp->s.next = NULL;
                    user_t->tail = tmp;
                }
                tmp = tmp->s.next;
            }
        }
        use_sbrk(user_t,0 - sizeof(header_t) - header->s.size);
        return;
    }
    header->s.is_free = 1;
}

