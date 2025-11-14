#pragma once

#define MAX_COMMAND_LEN 100
#define MAX_ARG_NR      50

bool exec(int argc, char **argv);
void mount(int argc, char **argv);

_Noreturn void shell_setup();
