#ifndef CPOS_SHELL_H
#define CPOS_SHELL_H

#define MAX_COMMAND_LEN 100
#define MAX_ARG_NR 50

char getc();
int gets(char *buf, int buf_size);
void setup_shell();

//内置命令
void end_echo(int argc,char **argv);
void cmd_ls();
void cmd_cat(int argc,char **argv);
void cmd_read(int argc,char **argv);

#endif