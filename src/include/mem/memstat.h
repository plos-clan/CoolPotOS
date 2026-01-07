#pragma once

#include "types.h"

uint64_t get_reserved_memory();

uint64_t get_all_memory();

uint64_t get_available_memory();

uint64_t get_used_memory();

uint64_t get_bad_memory();

size_t get_total_frames();
size_t get_origin_frames();
size_t get_usable_frames();
