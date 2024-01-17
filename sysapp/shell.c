#include "../include/shell.h"
#include "../include/multiboot.h"
#include "../include/vga.h"
#include "../include/queue.h"
#include "../include/keyboard.h"
#include "../include/io.h"
#include "../include/common.h"

extern Queue *key_char_queue;

char getc()
{
    while (key_char_queue->size == 0x00)
    {
        io_hlt();
    }
    return queue_pop(key_char_queue);
}

int gets(char *buf, int buf_size)
{
    int index = 0;
    char c;
    while ((c = getc()) != '\n')
    {
        if (c == '\b')
        {
            if (index > 0)
            {
                index--;
                vga_writestring("\b \b");
            }
        }
        else
        {
            buf[index++] =c;
            vga_putchar(c);
        }
    }
    buf[index] = '\0';
    vga_putchar(c);
    return index;
}

int cmd_parse(char *cmd_str,char **argv,char token)
{
    int arg_idx = 0;
    while (arg_idx < MAX_ARG_NR)
    {
        argv[arg_idx] = NULL;
        arg_idx++;
    }
    char *next = cmd_str;
    int argc = 0;

    while (*next)
    {
        while (*next == token) *next++;
        if(*next == 0) break;
        argv[argc] = next;
        while (*next && *next != token) *next++;
        if(*next) *next++ = 0;
        if(argc > MAX_ARG_NR) return -1;
        argc++;
    }
    
    return argc;
}

void cmd_echo(int argc,char **argv)
{
    for(int i = 1;i < argc;i++)
    {
        if(i==1) vga_writestring("");
        else vga_writestring(" ");
        vga_writestring(argv[i]);
    }
    vga_putchar('\n');
}

void setup_shell()
{
    vga_clear();
    printf("CrashPowerDOS for x86 [Version %s] \n", VERSION);
    printf("Copyright by XIAOYI12 [CrashPowerShell]\n");

    char com[MAX_COMMAND_LEN];
    char *argv[MAX_ARG_NR];
    int argc = -1;

    while(1)
    {
        printf("CPOS/> ");
        if(gets(com,MAX_COMMAND_LEN) <= 0) continue;
        argc = cmd_parse(com,argv,' ');

        if(argc == -1)
        {
            vga_writestring("[Shell]: Error: out of arguments buffer\n");
            continue;
        }

        if(!strcmp("version",argv[0]))
            printf("CrashPowerDOS for x86 [%s]\n",VERSION);
        else if(!strcmp("echo",argv[0]))
            cmd_echo(argc,argv);
        else if(!strcmp("clear",argv[0]))
            vga_clear();
        else if(!strcmp("help",argv[0])||!strcmp("?",argv[0])||!strcmp("h",argv[0]))
        {
            vga_writestring("-=[CrashPowerShell Helper]=-\n");
            vga_writestring("help ? h   empty   Print shell help info.\n");
            vga_writestring("version    empty   Print os version.\n");
            vga_writestring("echo       <msg>   Print message.\n");
        }
        else printf("[Shell]: Unknown command '%s'.\n",argv[0]);
    }
}