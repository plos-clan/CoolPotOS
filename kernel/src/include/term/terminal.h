#pragma once

#include "driver/tty.h"

void terminal_cols_rows(tty_t *session, size_t *cols, size_t *rows);
void terminal_width_height(tty_t *session, size_t *width, size_t *height);
int create_session_terminal(tty_t *session);
