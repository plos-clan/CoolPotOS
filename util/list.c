#include "../include/list.h"
#include "../include/memory.h"

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

void Change(size_t count, struct List* Obj, uintptr_t val) {
    struct List* Will_Change = FindForCount(count + 1, Obj);
    if (Will_Change != NULL) {
        Will_Change->val = val;
    } else {
        AddVal(val, Obj);
    }
}

void DeleteList(struct List* Obj) {
    Obj = Obj->ctl->start;
    kfree((void *)Obj->ctl);
    for (; Obj != (struct List*)NULL;) {
        //printf("Will free: %llx\n", Obj);
        struct List* tmp = Obj;
        Obj = Obj->next;
       kfree((void *)tmp);
    }
    return;
}

struct List* NewList() {
    struct List* Obj = (struct List*)kmalloc(sizeof(struct List));
    struct ListCtl* ctl = (struct ListCtl*)kmalloc(sizeof(struct ListCtl));
    Obj->ctl = ctl;
    Obj->ctl->start = Obj;
    Obj->ctl->end = Obj;
    Obj->val = 0x123456;  // 头结点数据不可用
    Obj->prev = (List*)NULL;
    Obj->next = (List*)NULL;
    Obj->ctl->all = 0;
    return Obj;
}

void DeleteVal(size_t count, struct List* Obj) {
    struct List* Will_Free = FindForCount(count, Obj);
    if (Will_Free == NULL) {
        // Not found!
        return;
    }
    if (count == 0) {
        return;
    }
    if (Will_Free->next == (List*)NULL) {
        // 是尾节点
        struct List* prev = FindForCount(count - 1, Obj);
        prev->next = (List*)NULL;
        prev->ctl->end = prev;
    } else {
        struct List* prev = FindForCount(count - 1, Obj);
        struct List* next = FindForCount(count + 1, Obj);
        prev->next = next;
        next->prev = prev;
    }
    kfree((void *)Will_Free);
    Obj->ctl->all--;
}

void AddVal(uintptr_t val, struct List* Obj) {
    while (Obj->next != NULL)
        Obj = Obj->next;
    Obj = Obj->ctl->end;
    struct List* new = (struct List*)kmalloc(sizeof(struct List));
    Obj->next = new;
    Obj->ctl->end = new;
    new->prev = Obj;
    new->ctl = Obj->ctl;
    new->next = (List*)NULL;
    new->val = val;
    new->ctl->all++;
}
