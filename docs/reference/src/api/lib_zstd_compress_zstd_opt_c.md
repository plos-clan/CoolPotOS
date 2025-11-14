# lib/zstd/compress/zstd_opt.c

## `static U32 ZSTD_insertBt1(const ZSTD_matchState_t *ms, const BYTE *const ip, const BYTE *const iend, U32 const target, U32 const mls, const int extDict) {`

ZSTD_insertBt1() : add one or multiple positions to tree.

- **`ip`**: assumed <= iend-8 .
- **`target`**: The target of ZSTD_updateTree_internal() - we are filling to this position
- **Returns**: : nb of positions added 

---

## `/* Struct containing info needed to make decision about ldm inclusion */ typedef struct {`

**********************
LDM helper functions  *
***********************

---

