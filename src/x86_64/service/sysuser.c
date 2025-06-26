#include "sysuser.h"
#include "heap.h"
#include "kprint.h"
#include "krlibc.h"
#include "tty.h"

ucb_t kernel_user   = NULL;
int   user_id_index = 0;

int    user_envc = 0;
char **user_envp;

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
    user->envc             = user_envc;
    user->envp             = user_envp;
    return user;
}

int add_env(const char *kv) {
    if (!kv || !strchr(kv, '=')) return -1;

    const char *eq      = strchr(kv, '=');
    size_t      key_len = eq - kv;

    for (int i = 0; i < user_envc; ++i) {
        if (strncmp(user_envp[i], kv, key_len) == 0 && user_envp[i][key_len] == '=') {
            free(user_envp[i]);
            user_envp[i] = strdup(kv);
            return 0;
        }
    }

    char **new_envp = realloc(user_envp, sizeof(char *) * (user_envc + 1));
    if (!new_envp) return -1;

    user_envp            = new_envp;
    user_envp[user_envc] = strdup(kv);
    if (!user_envp[user_envc]) return -1;

    user_envc++;
    return 0;
}

int del_env(const char *key) {
    if (!key) return -1;
    size_t key_len = strlen(key);

    for (int i = 0; i < user_envc; ++i) {
        if (strncmp(user_envp[i], key, key_len) == 0 && user_envp[i][key_len] == '=') {
            free(user_envp[i]);
            for (int j = i; j < user_envc - 1; ++j) {
                user_envp[j] = user_envp[j + 1];
            }
            user_envc--;

            if (user_envc == 0) {
                free(user_envp);
                user_envp = NULL;
            } else {
                char **new_envp = realloc(user_envp, sizeof(char *) * user_envc);
                if (new_envp) user_envp = new_envp;
            }
            return 0;
        }
    }

    return -1;
}

void user_setup() {
    kernel_user = (ucb_t)malloc(sizeof(struct user_control_block));
    if (kernel_user == NULL) {
        kerror("Kernel user malloc failed.");
        loop __asm__ volatile("hlt");
    }
    strcpy(kernel_user->name, "Kernel");
    add_env("PATH=/boot:/limine");
    add_env("HOME=/");
    kernel_user->uid              = user_id_index++;
    kernel_user->permission_level = Kernel;
    kernel_user->envc             = user_envc;
    kernel_user->envp             = user_envp;
    kinfo("User system setup (%s uid:%d).", kernel_user->name, kernel_user->uid);
}
