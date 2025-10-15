#include "psf.h"

bool check_psf_magic(psf_file_header_t *header){
    return header->magic == PSF_MAGIC;
}

uint8_t *get_psf_data(psf_file_header_t *header){
    return (uint8_t *)((uintptr_t)header + header->header_size);
}


