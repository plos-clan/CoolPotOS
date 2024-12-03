#pragma once

#define SEND_VERB_METHOD_DMA      1
#define SEND_VERB_METHOD_MMIO     2
#define HDA_WIDGET_AUDIO_OUTPUT   0x0
#define HDA_WIDGET_AUDIO_INPUT    0x1
#define HDA_WIDGET_MIXER          0x2
#define HDA_WIDGET_AUDIO_SELECTOR 0x3
#define HDA_WIDGET_PIN_COMPLEX    0x4
#define HDA_WIDGET_POWER_WIDGET   0x5
#define HDA_WIDGET_VKW            0x6
#define HDA_WIDGET_BG             0x7
#define HDA_WIDGET_VENDOR         0xF

#define HDA_PIN_COMPLEX_LINE_OUT           0x0
#define HDA_PIN_COMPLEX_SPEAKER            0x1
#define HDA_PIN_COMPLEX_HP_OUT             0x2
#define HDA_PIN_COMPLEX_CD                 0x3
#define HDA_PIN_COMPLEX_SPDIF_OUT          0x4
#define HDA_PIN_COMPLEX_DIG_OUT            0x5
#define HDA_PIN_COMPLEX_MODEM_LINE_SIDE    0x6
#define HDA_PIN_COMPLEX_MODEM_HANDSET_SIDE 0x7
#define HDA_PIN_COMPLEX_LINE_IN            0x8
#define HDA_PIN_COMPLEX_AUX                0x9
#define HDA_PIN_COMPLEX_MIC_IN             0xa
#define HDA_PIN_COMPLEX_TELEPHONY          0xb
#define HDA_PIN_COMPLEX_SPDIF_IN           0xc
#define HDA_PIN_COMPLEX_DIG_IN             0xd

#include "ctypes.h"

void hda_init();
uint32_t hda_verb(uint32_t codec, uint32_t node, uint32_t verb, uint32_t command);
void hda_init_audio_selector(uint32_t codec, uint32_t node);