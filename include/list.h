#ifndef CRASHPOWEROS_LIST_H
#define CRASHPOWEROS_LIST_H

#include <stdint.h>
#include <stddef.h>

struct ListCtl {
    struct List *start;
    struct List *end;
    int all;
};
struct List {
    struct ListCtl *ctl;
    struct List *prev;
    uintptr_t val;
    struct List *next;
};

typedef struct List List;

struct List* FindForCount(size_t count, struct List* Obj);
int GetLastCount(struct List* Obj);
void DeleteList(struct List* Obj);
struct List* NewList();
void Change(size_t count, struct List* Obj, uintptr_t val);
void DeleteVal(size_t count, struct List* Obj);
void AddVal(uintptr_t val, struct List* Obj);

#endif
