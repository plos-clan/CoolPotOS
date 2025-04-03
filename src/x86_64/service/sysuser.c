#include "sysuser.h"
#include "heap.h"
#include "kprint.h"
#include "krlibc.h"
#include "tty.h"

ucb_t kernel_user   = NULL;
int   user_id_index = 0;

ucb_t get_kernel_user() {
    return kernel_user;
}

ucb_t create_user(const char *name, UserLevel permission_level) {
    if (name == NULL) return NULL;
    ucb_t user = (ucb_t)malloc(sizeof(struct user_control_block));
    if (user == NULL) return NULL;
    strcpy(user->name, name);
    user->uid              = user_id_index++;
    user->permission_level = permission_level;
    return user;
}

void user_setup() {
    kernel_user = (ucb_t)malloc(sizeof(struct user_control_block));
    if (kernel_user == NULL) {
        kerror("Kernel user malloc failed.");
        infinite_loop __asm__ volatile("hlt");
    }
    strcpy(kernel_user->name, "Kernel");
    kernel_user->uid              = user_id_index++;
    kernel_user->permission_level = Kernel;
    kinfo("User system setup, (%s uid:%d).", kernel_user->name, kernel_user->uid);
}
