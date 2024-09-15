//
// This file is part of pl_readline.
// pl_readline is free software: you can redistribute it and/or modify
// it under the terms of MIT license.
// See file LICENSE or https://opensource.org/licenses/MIT for full license
// details.
//
// Copyright (c) 2024 min0911_ https://github.com/min0911Y
//

// plreadln.c: 实现pl_readline的核心功能
#include <assert.h>
#include "../include/pl_readline.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pl_readline_add_history(_SELF, char *line) {
    if (strlen(line))
        list_prepend(self->history, strdup(line));
    return PL_READLINE_SUCCESS;
}

pl_readline_t pl_readline_init(
        int (*pl_readline_hal_getch)(), void (*pl_readline_hal_putch)(int ch),
        void (*pl_readline_hal_flush)(),
        void (*pl_readline_get_words)(char *buf, pl_readline_words_t words)) {
    pl_readline_t plreadln = malloc(sizeof(struct pl_readline));
    if (!plreadln)
        return NULL;
    // 设置回调函数
    plreadln->pl_readline_hal_getch = pl_readline_hal_getch;
    plreadln->pl_readline_hal_putch = pl_readline_hal_putch;
    plreadln->pl_readline_hal_flush = pl_readline_hal_flush;
    plreadln->pl_readline_get_words = pl_readline_get_words;
    // 设置history链表
    plreadln->history = NULL;
    return plreadln;
}

void pl_readline_uninit(_SELF) {
    list_free_with(self->history, free);
    free(self);
}

void pl_readline_insert_char(char *str, char ch, int idx) {
    int len = strlen(str);
    memmove(str + idx + 1, str + idx, len - idx);
    str[idx] = ch;
}

static void pl_readline_delete_char(char *str, int idx) {
    int len = strlen(str);
    memmove(str + idx, str + idx + 1, len - idx);
    str[len] = '\0';
}

void pl_readline_print(_SELF, char *str) {
    while (*str) {
        self->pl_readline_hal_putch(*str++);
    }
}

static void pl_readline_reset(_SELF, int p, int len) {
    char buf[255] = {0};
    if (p) {
        sprintf(buf, "\e[%dD", p);
        pl_readline_print(self, buf);
    }
    if (len) {
        for (int i = 0; i < len; i++) {
            self->pl_readline_hal_putch(' ');
        }
        sprintf(buf, "\e[%dD", len);
        pl_readline_print(self, buf);
    }
}

static void pl_readline_to_the_end(_SELF, int n) {
    char buf[255] = {0};
    sprintf(buf, "\e[%dC", n);
    pl_readline_print(self, buf);
}

// 处理向上向下键
static void pl_readline_handle_key_down_up(_SELF, pl_readline_runtime *rt,
                                           int n) {
    list_t node = list_nth(self->history, rt->history_idx); // 获取历史记录
    if (!node) {
        rt->history_idx += n; // 超出历史记录的范围，回退到上一个记录
        return;
    }
    pl_readline_reset(self, rt->p, rt->length); // 重置光标和输入的字符
    self->pl_readline_hal_flush(); // 刷新输出缓冲区，在Linux下需要,否则会导致输入不显示
    rt->p = 0;                      // 光标移动到最左边
    rt->length = 0;                 // 清空缓冲区长度
    memset(rt->buffer, 0, rt->len); // 清空缓冲区
    strcpy(rt->buffer, node->data);
    pl_readline_print(self, rt->buffer); // 打印历史记录
    rt->length = strlen(rt->buffer);     // 更新缓冲区长度
    rt->p = rt->length;

    memset(rt->input_buf, 0, rt->len); // 清空输入缓冲区
    rt->input_buf_ptr = 0;             // 输入缓冲区指针置0
    // strcpy(rt->input_buf, node->data); // 复制历史记录到输入缓冲区
    char *p = node->data;
    while (*p) {
        if (*p == ' ') {
            rt->input_buf_ptr = 0;
            p++;
            continue;
        }
        rt->input_buf[rt->input_buf_ptr++] = *p++;
    }
    rt->input_buf[rt->input_buf_ptr] = '\0';
    rt->input_buf_ptr = strlen(rt->input_buf); // 更新输入缓冲区指针
}

void pl_readline_insert_char_and_view(_SELF, char ch, pl_readline_runtime *rt) {
    pl_readline_insert_char(rt->buffer, ch, rt->p++);
    rt->length++;
    int n = rt->length - rt->p;
    if (n) {
        char buf[255] = {0};
        pl_readline_print(self, rt->buffer + rt->p - 1);
        sprintf(buf, "\e[%dD", n);
        pl_readline_print(self, buf);

    } else {
        self->pl_readline_hal_putch(ch);
    }
}

void pl_readline_next_line(_SELF, pl_readline_runtime *rt) {
    int n = rt->length - rt->p;
    char buf[255] = {0};
    if (!n) {
        pl_readline_print(self, "\n");
        return;
    }
    sprintf(buf, "\e[%dC", n); // 光标移动到最右边
    pl_readline_print(self, buf);
    pl_readline_print(self, "\n");
}

// 处理输入的字符
int pl_readline_handle_key(_SELF, int ch, pl_readline_runtime *rt) {
    if (ch != PL_READLINE_KEY_TAB) {
        rt->intellisense_mode = false;
        if (rt->intellisense_word) {
            free(rt->intellisense_word);
            rt->intellisense_word = NULL;
        }
    }
    if (rt->length >= rt->len) { // 输入的字符数超过最大长度
        pl_readline_to_the_end(self, rt->length - rt->p);
        self->pl_readline_hal_putch('\n');
        rt->buffer[rt->length] = '\0';
        pl_readline_add_history(self, rt->buffer);
        return PL_READLINE_SUCCESS;
    }
    switch (ch) {
        case PL_READLINE_KEY_DOWN:
            rt->history_idx--;
            pl_readline_handle_key_down_up(self, rt, 1); // n = 1是为了的失败的时候还原
            break;
        case PL_READLINE_KEY_UP: {
            rt->history_idx++;
            // n = -1是为了的失败的时候还原
            pl_readline_handle_key_down_up(self, rt, -1);
            break;
        }
        case PL_READLINE_KEY_LEFT:
            if (!rt->p) // 光标在最左边
                return PL_READLINE_NOT_FINISHED;
            rt->p--;
            pl_readline_print(self, "\e[D");
            if (rt->buffer[rt->p] == ' ') {
                memset(rt->input_buf, 0, rt->len);
                // 光标移动到前一个空格
                int i = rt->p;
                while (i && rt->buffer[i - 1] != ' ') {
                    i--;
                }
                rt->input_buf_ptr = 0;
                // 从i开始复制到rt->input_buf，直到遇到空格
                int len = rt->p - i;
                while (i < rt->p && rt->buffer[i] != ' ') {
                    rt->input_buf[rt->input_buf_ptr++] = rt->buffer[i];
                    i++;
                }
                // 字符串结束符号
                rt->input_buf[rt->input_buf_ptr] = '\0';
            } else {
                rt->input_buf_ptr--;
            }
            break;
        case PL_READLINE_KEY_RIGHT:
            if (rt->p == rt->length) // 光标在最右边
                return PL_READLINE_NOT_FINISHED;
            rt->p++;
            pl_readline_print(self, "\e[C");
            if (rt->buffer[rt->p - 1] == ' ') {
                memset(rt->input_buf, 0, rt->len);
                // 光标移动到前一个空格
                int i = rt->p;
                int j = i;
                while (i < rt->length && rt->buffer[i + 1] != ' ') {
                    i++;
                }
                rt->input_buf_ptr = 0;
                // 从i开始复制到rt->input_buf，直到遇到空格
                int len = i - j;
                while (j <= i && rt->buffer[i] != ' ') {
                    rt->input_buf[rt->input_buf_ptr++] = rt->buffer[j];
                    j++;
                }
                // 字符串结束符号
                rt->input_buf[rt->input_buf_ptr] = '\0';
                rt->input_buf_ptr = 0;
            } else {
                rt->input_buf_ptr++;
            }
            break;
        case PL_READLINE_KEY_BACKSPACE:
            if (!rt->p) // 光标在最左边
                return PL_READLINE_NOT_FINISHED;
            --rt->p;
            if (rt->buffer[rt->p] == ' ') {
                memset(rt->input_buf, 0, rt->len);
                // 光标移动到前一个空格
                int i = rt->p;
                while (i && rt->buffer[i - 1] != ' ') {
                    i--;
                }
                rt->input_buf_ptr = 0;
                // 从i开始复制到rt->input_buf，直到遇到空格
                while (i < rt->p && rt->buffer[i] != ' ') {
                    rt->input_buf[rt->input_buf_ptr++] = rt->buffer[i];
                    i++;
                }
                // 字符串结束符号
                rt->input_buf[rt->input_buf_ptr] = '\0';
            } else {
                if (rt->input_buf_ptr)
                    pl_readline_delete_char(rt->input_buf, --rt->input_buf_ptr);
            }
            pl_readline_delete_char(rt->buffer, rt->p);

            rt->length--;

            int n = rt->length - rt->p;
            if (n) {
                char buf[255] = {0};
                sprintf(buf, "\e[%dC\e[D ", n);
                pl_readline_print(self, buf);

                sprintf(buf, "\e[%dD", n);
                pl_readline_print(self, buf);
                pl_readline_print(self, "\e[D");
                pl_readline_print(self, rt->buffer + rt->p);
                pl_readline_print(self, buf);

            } else {
                pl_readline_print(self, "\e[D \e[D");
            }
            break;
        case PL_READLINE_KEY_ENTER:
            pl_readline_to_the_end(self, rt->length - rt->p);
            self->pl_readline_hal_putch('\n');
            rt->buffer[rt->length] = '\0';
            pl_readline_add_history(self, rt->buffer);
            return PL_READLINE_SUCCESS;
        case PL_READLINE_KEY_TAB: { // 自动补全
            pl_readline_words_t words = pl_readline_word_maker_init();
            pl_readline_word word_seletion = pl_readline_intellisense(self, rt, words);
            if (word_seletion.word) {
                pl_readline_intellisense_insert(self, rt, word_seletion);
                self->pl_readline_hal_flush();
            } else if (word_seletion.first) {
                pl_readline_print(self, "\n");
                pl_readline_print(self, rt->prompt);
                pl_readline_print(self, rt->buffer);
                int n = rt->length - rt->p;
                char buf[255] = {0};
                if (n) {
                    sprintf(buf, "\e[%dD", n);
                    pl_readline_print(self, buf);
                }
                self->pl_readline_hal_flush();
            }
            break;
        }
        case ' ': {
            memset(rt->input_buf, 0, rt->len);
            rt->input_buf_ptr = 0;
            goto handle;
        }
        default: {
            pl_readline_insert_char(rt->input_buf, ch, rt->input_buf_ptr++);
            handle:
            pl_readline_insert_char_and_view(self, ch, rt);
            break;
        }
    }
    return PL_READLINE_NOT_FINISHED;
}

// 主体函数
int pl_readline(_SELF, char *prompt, char *buffer, size_t len) {
    // 为了实现自动补全，需要将输入的字符保存到缓冲区中
    char *input_buf = malloc(len + 1);
    memset(input_buf, 0, len + 1);
    int input_buf_ptr = 0;
    assert(input_buf);
    pl_readline_runtime rt = {buffer, 0, 0, -1, prompt,
                              len, input_buf, 0, false, NULL};

    // 清空缓冲区
    memset(input_buf, 0, len + 1);
    memset(buffer, 0, len);
    // 打印提示符
    pl_readline_print(self, prompt);
    self->pl_readline_hal_flush(); // 刷新输出缓冲区，在Linux下需要,否则会导致输入不显示
    // 循环读取输入
    while (true) {
        int ch = self->pl_readline_hal_getch(); // 读取输入
        int status = pl_readline_handle_key(self, ch, &rt);
        if (status == PL_READLINE_SUCCESS) {
            break;
        }
    }
    free(input_buf);
    if (rt.intellisense_word) {
        free(rt.intellisense_word);
    }
    return PL_READLINE_SUCCESS;
}
