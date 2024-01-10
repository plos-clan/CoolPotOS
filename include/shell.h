#ifndef CPOS_SHELL_H
#define CPOS_SHELL_H

#define MAX_COMMAND_LEN 100
#define MAX_ARG_NR 50

char getc();
int gets(char *buf, int buf_size);
int cmd_parse(char *cmd_str, char **argv, char token);

void cmd_echo(int argc, char **argv);
void cmd_ls(int argc, char **argv);
void cmd_exit();

void shell_setup();

#endif
