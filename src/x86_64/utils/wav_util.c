#include "wav_util.h"
#include "krlibc.h"

int extract_wav_audio_data(cp_module_t *mod, uint8_t **audio_data, size_t *audio_size) {
    if (!mod || !audio_data || !audio_size || mod->size < 44) return -1;

    uint8_t *ptr = mod->data;
    uint8_t *end = mod->data + mod->size;

    if (memcmp(ptr, "RIFF", 4) != 0 || memcmp(ptr + 8, "WAVE", 4) != 0) return -1;
    ptr += 12;

    while (ptr + 8 <= end) {
        uint32_t chunk_id = *(uint32_t*)ptr;
        uint32_t chunk_size = *(uint32_t*)(ptr + 4);

        if (chunk_id == 0x20746D66) {
            if (chunk_size < 16) return -1;
            // TODO 解析 fmt 子块内容
        }

        ptr += 8 + chunk_size;
        if (ptr > end) return -1;
    }

    ptr = mod->data + 12;
    while (ptr + 8 <= end) {
        uint32_t chunk_id = *(uint32_t*)ptr;
        uint32_t chunk_size = *(uint32_t*)(ptr + 4);
        uint8_t *chunk_data = ptr + 8;

        if (memcmp(ptr, "data", 4) == 0) {
            if (chunk_data + chunk_size > end) return -1;
            *audio_data = chunk_data;
            *audio_size = chunk_size;
            return 0;
        }

        ptr += 8 + chunk_size;
        if (ptr > end) return -1;
    }

    return -1;
}
