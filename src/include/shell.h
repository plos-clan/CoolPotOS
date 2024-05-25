#ifndef CRASHPOWEROS_SHELL_H
#define CRASHPOWEROS_SHELL_H

#define MAX_COMMAND_LEN 100
#define MAX_ARG_NR 50

char getc();
int gets(char *buf, int buf_size);
void setup_shell();

//内置命令
void cmd_echo(int argc,char **argv);
void cmd_proc(int argc, char **argv);
void cmd_date();
void cmd_reset();
void cmd_del(int argc, char **argv);
void cmd_mkdir(int argc, char **argv);
void cmd_read(int argc, char **argv);
void cmd_cat(int argc, char **argv);
void cmd_ls();
void cmd_shutdown();
void cmd_debug();

#endif //CRASHPOWEROS_SHELL_H
