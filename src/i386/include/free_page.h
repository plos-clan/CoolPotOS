#pragma once

#define MAX_FREE_QUEUE 64

#include "page.h"

void free_pages();
void put_directory(page_directory_t *dir);
void setup_free_page();