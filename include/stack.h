#ifndef CPOS_STACK_H
#define CPOS_STACK_H

#define MAX_STACK_SIZE 20

typedef struct {
    int top;
    int size;
    char **array;
}Stack;

Stack *create_stack();
void push_stack(Stack *stack,char* data);
char* pop_stack(Stack *stack);
char* peek_stack(Stack *stack);
int empty_stack(Stack *stack);
void free_stack(Stack *stack);

#endif
