//
// This file is part of pl_readline.
// pl_readline is free software: you can redistribute it and/or modify
// it under the terms of MIT license.
// See file LICENSE or https://opensource.org/licenses/MIT for full license
// details.
//
// Copyright (c) 2024 min0911_ https://github.com/min0911Y
//

// pl_readline_intellisense.c : Intellisense feature for pl_readline.

#include <assert.h>
#include "../include/pl_readline.h"
#include <stdio.h>
#include <string.h>

// 向输入缓冲区插入字符
static void insert_char(_SELF, char ch, pl_readline_runtime *rt) {
    pl_readline_insert_char_and_view(self, ch, rt);
    pl_readline_insert_char(rt->input_buf, ch, rt->input_buf_ptr++);
}

// 向输入缓冲区插入字符串
static void insert_string(_SELF, char *str, pl_readline_runtime *rt) {
    while (*str) {
        insert_char(self, *str++, rt);
    }
}

// 找到相同前缀的单词
// 使用此函数需要记得free返回的指针
static char *get_the_same_prefix_in_words(_SELF, pl_readline_words_t words) {
    char *prefix = NULL;
    int len = 0;
    for (int i = 0; i < words->len; i++) {
        if (prefix == NULL) {
            prefix = strdup(words->words[i].word);
            len = strlen(prefix);
        } else {
            int j = 0;
            while (j < len && j < strlen(words->words[i].word) &&
                   prefix[j] == words->words[i].word[j]) {
                j++;
            }
            if (j < len) {
                prefix = realloc(prefix, j + 1);
                len = j;
                prefix[j] = '\0';
            }
        }
    }
    return prefix;
}

// 检查是否是第一个单词
static bool check_if_is_first(_SELF, pl_readline_runtime *rt) {
    int p = rt->p;
    while (p) {
        if (rt->buffer[--p] == ' ') { // 看一下前面有没有空格
            return false;               // 不是第一个单词
        }
    }
    return true;
}

// 自动补全
pl_readline_word pl_readline_intellisense(_SELF, pl_readline_runtime *rt,
                                          pl_readline_words_t words) {
    char *buf;                                   // 用户输入的缓冲区
    int times = 0;                               // 输出补全词库的次数
    int idx;                                     // rt->intellisense_word的索引
    int flag = 0;                                // 用户输入的词存不存在
    bool is_first = check_if_is_first(self, rt); // 是否是第一个单词
    if (rt->intellisense_mode == false) { // 如果是这个模式，则我们需要插入些东西
        buf = strdup(rt->input_buf); // 保存一下
        // rt->intellisense_word将会在后面被释放，不用担心内存泄漏
        rt->intellisense_word = buf;
        buf[rt->input_buf_ptr] = '\0';         // 加上结束符
        idx = rt->input_buf_ptr;               // 索引
    } else {                                 // 列出模式
        assert(rt->intellisense_word != NULL); // 这个指针一定不为空
        buf = rt->intellisense_word;           // 重定向buf
        idx = strlen(buf);                     // 设置索引
    }
    self->pl_readline_get_words(buf, words); // 请求词库
    int can_be_selected = 0;                 // 可能的单词数
    char sep;                                // 用于分隔符
    for (int i = 0; i < words->len; i++) {
        if (strncmp(buf, words->words[i].word, idx) == 0 &&
            (is_first || !words->words[i].first)) { // 找到相同前缀的单词
            if (strlen(words->words[i].word) >
                rt->input_buf_ptr) // 找到的单词比输入的长
                can_be_selected++;   // 可能的单词数加一
            if (strlen(words->words[i].word) ==
                rt->input_buf_ptr) { // 找到的单词和输入的一样长
                char *temp_buffer = strdup(rt->input_buf); // 临时缓冲区
                temp_buffer[rt->input_buf_ptr] = '\0';     // 加上结束符
                if (strcmp(words->words[i].word, temp_buffer) ==
                    0) {                     // 找到的单词和输入一样
                    flag = 1;                  // flag代表有这个词
                    sep = words->words[i].sep; // 用于分隔符
                } else {
                    can_be_selected++; // 可能的单词数加一
                }
                free(temp_buffer); // 释放临时缓冲区
            }
        }
    }
    // 该单词在词库中只有一个可匹配项，那么不需要补全
    if (can_be_selected == 0 && flag == 1) {
        pl_readline_word_maker_destroy(words); // 释放words
        if (sep)                               // 有分隔符，插入一下即可
            pl_readline_handle_key(self, sep, rt);
        return (pl_readline_word) {0};
    }

    pl_readline_words_t words_temp = NULL;
    if (!rt->intellisense_mode) {
        words_temp =
                pl_readline_word_maker_init(); // 临时词库，用于存储“可能”的单词
        if (idx == 0) {                    // 没东西我们直接输出词库
            goto NOTHING_INPUT;
        }
    }
    for (int i = 0; i < words->len; i++) {
        if (strncmp(buf, words->words[i].word, idx) == 0 &&
            (is_first || !words->words[i].first)) {
            // 和用户输入一样的单词有什么好必要补全的？
            if (!rt->intellisense_mode && strlen(words->words[i].word) == idx) {
                // 直接下一个
                continue;
            }
            if (!rt->intellisense_mode) { // 如果是需要输出模式
                pl_readline_word_maker_add(words->words[i].word, words_temp, false,
                                           0); // 加入临时词库
            } else {
                if (times == 0) {                  // 第一次输出
                    pl_readline_next_line(self, rt); // 换行
                }
                if (times)                                     // 不是第一次输出
                    pl_readline_print(self, " ");                // 空格分隔
                pl_readline_print(self, words->words[i].word); // 输出单词
                times++;                                       // 输出次数加一
            }
        }
    }
    pl_readline_word ret = {0};
    if (rt->intellisense_mode == false) {
        // 找到相同前缀的单词
        // 作者按：这里get_the_same_prefix_in_words不可能找不到相同前缀的单词，
        // 因为words_temp里只有可能的单词，那么，天然的，他们已经有相同的前缀，只是多与少而已。
        // 但prefix仍然有可能为NULL,即：words_temp里没有单词。
        char *prefix = get_the_same_prefix_in_words(self, words_temp);
        /*
           找到的相同前缀和输入的一样 那么就是什么都没有改变，因而直接输出补全列表
        */
        if (prefix && strcmp(prefix, buf) == 0) {
            free(prefix);
            NOTHING_INPUT:
            rt->intellisense_mode = true;               // 设置为列出模式
            pl_readline_word_maker_destroy(words_temp); // 释放words_temp
            pl_readline_word_maker_destroy(words);      // 释放words
            words = pl_readline_word_maker_init();      // 重新初始化words

            return pl_readline_intellisense(self, rt, words);
        }
        // 若prefix为NULL，则不会去插入，这是调用者决定的，我这里不管
        ret.word = prefix;
        rt->intellisense_mode = true;
        pl_readline_word_maker_destroy(words_temp);
    }
    if (rt->intellisense_mode && times) {
        ret.first = true;
    }
    pl_readline_word_maker_destroy(words); // 释放words
    return ret;
}

void pl_readline_intellisense_insert(_SELF, pl_readline_runtime *rt,
                                     pl_readline_word word) {
    insert_string(self, word.word + rt->input_buf_ptr, rt);
    free(word.word);
}