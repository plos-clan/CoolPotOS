#include "vsound.h"

// 是否为有符号
bool _sound_fmt_issigned[] = {
        //- 8bit / 1byte
        true,
        false,
        //- 16bit / 2byte
        true,
        true,
        false,
        false,
        //- 24bit / 3byte
        true,
        true,
        false,
        false,
        //- 24bit / 4byte(low 3byte)
        true,
        true,
        false,
        false,
        //- 32bit / 4byte
        true,
        true,
        false,
        false,
        //- 64bit / 8byte
        true,
        true,
        false,
        false,
        //- 16bit / 2byte  <- float
        true,
        true,
        //- 32bit / 4byte  <- float
        true,
        true,
        //- 64bit / 8byte  <- float
        true,
        true,
};
// 是否为浮点
bool _sound_fmt_isfloat[] = {
        //- 8bit / 1byte
        false,
        false,
        //- 16bit / 2byte
        false,
        false,
        false,
        false,
        //- 24bit / 3byte
        false,
        false,
        false,
        false,
        //- 24bit / 4byte(low 3byte)
        false,
        false,
        false,
        false,
        //- 32bit / 4byte
        false,
        false,
        false,
        false,
        //- 64bit / 8byte
        false,
        false,
        false,
        false,
        //- 16bit / 2byte  <- float
        true,
        true,
        //- 32bit / 4byte  <- float
        true,
        true,
        //- 64bit / 8byte  <- float
        true,
        true,
};
// 是否为大端序
bool _sound_fmt_isbe[] = {
        //- 8bit / 1byte
        false,
        false,
        //- 16bit / 2byte
        false,
        true,
        false,
        true,
        //- 24bit / 3byte
        false,
        true,
        false,
        true,
        //- 24bit / 4byte(low 3byte)
        false,
        true,
        false,
        true,
        //- 32bit / 4byte
        false,
        true,
        false,
        true,
        //- 64bit / 8byte
        false,
        true,
        false,
        true,
        //- 16bit / 2byte  <- float
        false,
        true,
        //- 32bit / 4byte  <- float
        false,
        true,
        //- 64bit / 8byte  <- float
        false,
        true,
};
// 大小
int _sound_fmt_bytes[] = {
        //- 8bit / 1byte
        1,
        1,
        //- 16bit / 2byte
        2,
        2,
        2,
        2,
        //- 24bit / 3byte
        3,
        3,
        3,
        3,
        //- 24bit / 4byte(low 3byte)
        4,
        4,
        4,
        4,
        //- 32bit / 4byte
        4,
        4,
        4,
        4,
        //- 64bit / 8byte
        8,
        8,
        8,
        8,
        //- 16bit / 2byte  <- float
        2,
        2,
        //- 32bit / 4byte  <- float
        4,
        4,
        //- 64bit / 8byte  <- float
        8,
        8,
};

bool sound_fmt_issigned(sound_pcmfmt_t fmt) {
    if (fmt < 0 || fmt >= SOUND_FMT_CNT) return false;
    return _sound_fmt_issigned[fmt];
}

bool sound_fmt_isfloat(sound_pcmfmt_t fmt) {
    if (fmt < 0 || fmt >= SOUND_FMT_CNT) return false;
    return _sound_fmt_isfloat[fmt];
}

bool sound_fmt_isbe(sound_pcmfmt_t fmt) {
    if (fmt < 0 || fmt >= SOUND_FMT_CNT) return false;
    return _sound_fmt_isbe[fmt];
}

int sound_fmt_bytes(sound_pcmfmt_t fmt) {
    if (fmt < 0 || fmt >= SOUND_FMT_CNT) return -1;
    return _sound_fmt_bytes[fmt];
}
