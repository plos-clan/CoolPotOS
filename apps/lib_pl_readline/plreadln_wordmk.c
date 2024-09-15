//
// This file is part of pl_readline.
// pl_readline is free software: you can redistribute it and/or modify
// it under the terms of MIT license.
// See file LICENSE or https://opensource.org/licenses/MIT for full license
// details.
//
// Copyright (c) 2024 min0911_ https://github.com/min0911Y
//

// plreadln_wordmk.c: pl_readline word maker
#include <assert.h>
#include "../include/pl_readline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

pl_readline_words_t pl_readline_word_maker_init() {
    pl_readline_words_t words = malloc(sizeof(struct pl_readline_words));
    words->len = 0;      // initial length
    words->max_len = 16; // initial max length
    words->words = malloc(words->max_len * sizeof(pl_readline_word));
    assert(words->words != NULL);
    return words;
}

void pl_readline_word_maker_destroy(pl_readline_words_t words) {
    for (int i = 0; i < words->len; i++) {
        char *p = words->words[i].word;
        free(p);
    }
    free(words->words);
    free(words);
}

int pl_readline_word_maker_add(char *word, pl_readline_words_t words,
                               bool is_first, char sep) {
    if (words->len >= words->max_len) {
        words->max_len *= 2;
        words->words =
                realloc(words->words, words->max_len * sizeof(pl_readline_word));
        assert(words->words != NULL);
    }
    words->words[words->len].first = is_first;
    words->words[words->len].word = strdup(word);
    words->words[words->len].sep = sep;
    words->len++;
    return 0;
}

void pl_readline_word_maker_clear(pl_readline_words_t words) {
    for (int i = 0; i < words->len; i++) {
        free(words->words[i].word);
    }
    words->len = 0;
}