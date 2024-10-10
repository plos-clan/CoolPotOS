#include <stdio.h>
#include "../include/syscall.h"
#include "../include/string.h"
#include "../include/cpos.h"
#include "../include/pl_readline.h"
#include "../include/ttfprint.h"
#include "../include/image.h"

extern void print_info();

static int gets(char *buf) {

    int index = 0;
    char c;

    while ((c = getc()) != '\n') {
        if (c == '\b') {
            if (index > 0) {
                index--;
                printf("\b \b");
            }
        } else {
            buf[index++] = c;
            printf("%c",c);
        }
    }
    buf[index] = '\0';
    printf("%c",c);
    return index;
}

static inline int cmd_parse(char *cmd_str, char **argv, char token) {
    int arg_idx = 0;
    while (arg_idx < 50) {
        argv[arg_idx] = NULL;
        arg_idx++;
    }
    char *next = cmd_str;
    int argc = 0;

    while (*next) {
        while (*next == token) *next++;
        if (*next == 0) break;
        argv[argc] = next;
        while (*next && *next != token) *next++;
        if (*next) *next++ = 0;
        if (argc > 50) return -1;
        argc++;
    }

    return argc;
}

static void flush() { fflush(stdout); }

static void handle_tab(char *buf, pl_readline_words_t words) {
    pl_readline_word_maker_add("help", words, true, ' ');
    pl_readline_word_maker_add("version", words, false, ' ');
    pl_readline_word_maker_add("system", words, false, ' ');
    pl_readline_word_maker_add("image", words, false, ' ');
    pl_readline_word_maker_add("cd", words, false, ' ');
}

static int pl_getch(void) {
    int fd = 0, ch;

    ch = syscall_getch();

    if (ch == 0x0d) {
        return PL_READLINE_KEY_ENTER;
    }
    if (ch == 0x7f) {
        return PL_READLINE_KEY_BACKSPACE;
    }
    if (ch == 0x9) {
        return PL_READLINE_KEY_TAB;
    }
    if (ch == 0x1b) {
        ch = getch();
        if (ch == 0x5b) {
            ch = getch();
            switch (ch) {
                case 0x41:
                    return PL_READLINE_KEY_UP;
                case 0x42:
                    return PL_READLINE_KEY_DOWN;
                case 0x43:
                    return PL_READLINE_KEY_RIGHT;
                case 0x44:
                    return PL_READLINE_KEY_LEFT;
                default:
                    return -1;
            }
        }
    }
    return ch;
}

int main(int argc_v,char **argv_v){

    ttf_install("logo.ttf");
    print_ttf("CoolPotOS v0.3.3",0xffffff,0x000000,440,500,70.0);
    draw_image_xy("icon.png",490,180);

    //pl_readline_t n = pl_readline_init(pl_getch, (void *)put_char, flush, handle_tab);

    printf("Welcome to CoolPotOS UserShell v0.0.1\n");
    printf("Copyright by \033[1m\033[4mXIAOYI12\033[0m 2023-2024\n");
    char *com[100];
    char *argv[50];
    int argc = -1;
    char *buffer[255];

    while (1){
        syscall_get_cd(buffer);
        printf("\033[32m\033[7mdefault@localhost:\033[7m \033[34m%s\\\033[39m$ ",buffer);

        if (gets(com) <= 0) continue;
        /*
        free(com);
        com = malloc(100 * sizeof(char));
        pl_readline(n,"", com, 100);
         */

        char* com_copy[100];
        strcpy(com_copy,com);

        argc = cmd_parse(com, argv, ' ');

        if (argc == -1) {
            printf("[Shell]: Error: out of arguments buffer\n");
            continue;
        }

        if (!strcmp("version", argv[0])){
            print_info();
        }else if (!strcmp("system", argv[0])){
            exit(0);
        }else if (!strcmp("cd", argv[0])){
            if (argc == 1) {
                print("[Shell-EXEC]: If there are too few parameters, please specify the path.\n");
            } else syscall_vfs_change_path(argv[1]);
        }else if (!strcmp("image", argv[0])){
            if (argc == 1) {
                print("[Shell-EXEC]: If there are too few parameters, please specify the path.\n");
            } else draw_image(argv[1]);
        }else if (!strcmp("help", argv[0]) || !strcmp("?", argv[0]) || !strcmp("h", argv[0])) {
            printf("-=[CoolPotShell Helper]=-\n");
            printf("help ? h              Print shell help info.\n");
            printf("version               Print os version.\n");
            printf("system                Launch system shell.\n");
            printf("cd        <path>      Change directory.\n");
            printf("image     <filename>  Draw a image.\n");
        } else {
            int pid = exec_elf(argv[0],com_copy,false);

            if(pid == NULL){
                printf("\033[31m[Shell]: Unknown command '%s'.\033[39m\n", argv[0]);
            }
        }
    }
}