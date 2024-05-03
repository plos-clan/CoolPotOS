#include "../include/list.h"

int GetLastCount(struct List* Obj) {
    return Obj->ctl->all;
}

struct List* FindForCount(size_t count, struct List* Obj) {
    int count_last = GetLastCount(Obj);
    struct List *p = Obj, *q = Obj->ctl->end;
    if (count > count_last)
        return (List*)NULL;
    for (int i = 0, j = count_last;; i++, j--) {
        if (i == count) {
            return p;
        } else if (j == count) {
            return q;
        }
        p = p->next;
        q = q->prev;
    }
}
