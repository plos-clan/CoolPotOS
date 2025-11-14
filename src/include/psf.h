#pragma once

#define PSF_MAGIC 0x864ab572

#include "cptype.h"

typedef struct psf_file_header {
    uint32_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t flags;
    uint32_t num_glyph;
    uint32_t bytes_per_glyph;
    uint32_t height;
    uint32_t width;
}psf_file_header_t;

bool check_psf_magic(psf_file_header_t *header);

uint8_t *get_psf_data(psf_file_header_t *header);
