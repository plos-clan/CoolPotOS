#include "../include/pcat.h"
#include "../include/keyboard.h"
#include <stddef.h>

struct task_struct *father_pcat;
struct pcat_process *this_process;
uint32_t pid_pcat;
extern KEY_STATUS *key_status;

void pcat_key_listener(uint32_t key,int release,char c){

}

void start_pcat_thread(){
    this_process = (struct pcat_process*) kmalloc(sizeof(struct pcat_process));
    struct key_listener *listener = (struct key_listener*) kmalloc(sizeof(struct key_listener));
    listener->func = pcat_key_listener;
    listener->func = 2;
    add_listener(listener);
    while (1){
        if(key_status->is_ctrl){
            if(this_process->is_e){
                kfree(this_process);
                start_task(father_pcat);
                task_kill(get_current());
            }
        }


    }
}

void pcat_launch(struct task_struct *father,struct File *file){
    father_pcat = father;
    pid_pcat = kernel_thread(start_pcat_thread,NULL,"CPOS-PCAT");
    wait_task(father);
    while (1){
        if(father->state != TASK_SLEEPING) break;
    }
}