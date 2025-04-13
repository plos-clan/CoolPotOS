#pragma once

#include "ctype.h"
#include "module.h"

int extract_wav_audio_data(cp_module_t *mod, uint8_t **audio_data, size_t *audio_size);
