#include "../include/stack.h"
#include "../include/kheap.h"
#include "../include/redpane.h"

Stack *create_stack(){
    Stack *stack = (Stack*) alloc(sizeof(Stack));
    stack->top = 0;
    stack->size = 0;
    stack->array = (char **) alloc(sizeof(char) * MAX_STACK_SIZE);
    return stack;
}

void push_stack(Stack *stack,char * data){
    if(stack->top > MAX_STACK_SIZE)
        show_VGA_red_pane("STACK_OVER_FLOW","kernel stack over flow.",stack->top,NULL);
    stack->array[stack->top] = data;
    stack->top++;
    stack->size++;
}

char* pop_stack(Stack *stack){
    if(stack->size == 0)
        show_VGA_red_pane("EMPTY_STACK","kernel stack is empty",0x00,NULL);

    stack->top--;
    stack->size--;
    return stack->array[stack->top];
}

char* peek_stack(Stack *stack){
    if(stack->size == 0)
        show_VGA_red_pane("EMPTY_STACK","kernel stack is empty",0x00,NULL);
    return stack->array[stack->top-1];
}

int empty_stack(Stack *stack){
    return stack->size == 0;
}

void free_stack(Stack *stack){
    kfree(stack->array);
    kfree(stack);
}


