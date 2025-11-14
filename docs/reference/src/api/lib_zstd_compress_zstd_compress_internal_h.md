# lib/zstd/compress/zstd_compress_internal.h

## `typedef struct {`

ZSTD_hufCTablesMetadata_t :
Stores Literals Block Type for a super-block in hType, and
huffman tree description in hufDesBuffer.
hufDesSize refers to the size of huffman tree description in bytes.
This metadata is populated in ZSTD_buildBlockEntropyStats_literals() 

---

## `typedef struct {`

ZSTD_fseCTablesMetadata_t :
Stores symbol compression modes for a super-block in {ll, ol, ml}Type, and
fse tables in fseTablesBuffer.
fseTablesSize refers to the size of fse tables in bytes.
This metadata is populated in ZSTD_buildBlockEntropyStats_sequences() 

---

## `size_t ZSTD_buildBlockEntropyStats(seqStore_t* seqStorePtr, const ZSTD_entropyCTables_t* prevEntropy, ZSTD_entropyCTables_t* nextEntropy, const ZSTD_CCtx_params* cctxParams, ZSTD_entropyCTablesMetadata_t* entropyMetadata, void* workspace, size_t wkspSize);`

ZSTD_buildBlockEntropyStats() :
Builds entropy for the block.

- **Returns**: : 0 on success or error code 

---

## `typedef struct {`

******************************
Compression internals structs *
*******************************

---

## `typedef enum {`


Indicates whether this compression proceeds directly from user-provided
source buffer to user-provided destination buffer (ZSTDb_not_buffered), or
whether the context needs to buffer the input/output (ZSTDb_buffered).


---

## `#define ZSTD_MAX_NB_BLOCK_SPLITS 196 typedef struct {`


Struct that contains all elements of block splitter that should be allocated
in a wksp.


---

## `MEM_STATIC size_t ZSTD_count_2segments(const BYTE* ip, const BYTE* match, const BYTE* iEnd, const BYTE* mEnd, const BYTE* iStart) {`

ZSTD_count_2segments() :
can count match length with `ip` & `match` in 2 different segments.
convention : on reaching mEnd, match count continue starting from iStart


---

## `static U64 ZSTD_ipow(U64 base, U64 exponent) {`

ZSTD_ipow() :
Return base^exponent.


---

## `static U64 ZSTD_rollingHash_append(U64 hash, void const* buf, size_t size) {`

ZSTD_rollingHash_append() :
Add the buffer to the hash value.


---

## `MEM_STATIC U64 ZSTD_rollingHash_compute(void const* buf, size_t size) {`

ZSTD_rollingHash_compute() :
Compute the rolling hash value of the buffer.


---

## `MEM_STATIC U64 ZSTD_rollingHash_primePower(U32 length) {`

ZSTD_rollingHash_primePower() :
Compute the primePower to be passed to ZSTD_rollingHash_rotate() for a hash
over a window of length bytes.


---

## `MEM_STATIC U64 ZSTD_rollingHash_rotate(U64 hash, BYTE toRemove, BYTE toAdd, U64 primePower) {`

ZSTD_rollingHash_rotate() :
Rotate the rolling hash by one byte.


---

## `MEM_STATIC void ZSTD_window_clear(ZSTD_window_t* window) {`


ZSTD_window_clear():
Clears the window containing the history by simply setting it to empty.


---

## `MEM_STATIC U32 ZSTD_window_hasExtDict(ZSTD_window_t const window) {`


ZSTD_window_hasExtDict():
Returns non-zero if the window has a non-empty extDict.


---

## `MEM_STATIC ZSTD_dictMode_e ZSTD_matchState_dictMode(const ZSTD_matchState_t *ms) {`


ZSTD_matchState_dictMode():
Inspects the provided matchState and figures out what dictMode should be
passed to the compressor.


---

## `MEM_STATIC U32 ZSTD_window_canOverflowCorrect(ZSTD_window_t const window, U32 cycleLog, U32 maxDist, U32 loadedDictEnd, void const* src) {`


ZSTD_window_canOverflowCorrect():
Returns non-zero if the indices are large enough for overflow correction
to work correctly without impacting compression ratio.


---

## `MEM_STATIC U32 ZSTD_window_needOverflowCorrection(ZSTD_window_t const window, U32 cycleLog, U32 maxDist, U32 loadedDictEnd, void const* src, void const* srcEnd) {`


ZSTD_window_needOverflowCorrection():
Returns non-zero if the indices are getting too large and need overflow
protection.


---

## `MEM_STATIC U32 ZSTD_window_correctOverflow(ZSTD_window_t* window, U32 cycleLog, U32 maxDist, void const* src) {`


ZSTD_window_correctOverflow():
Reduces the indices to protect from index overflow.
Returns the correction made to the indices, which must be applied to every
stored index.

The least significant cycleLog bits of the indices must remain the same,
which may be 0. Every index up to maxDist in the past must be valid.


---

## `MEM_STATIC void ZSTD_window_enforceMaxDist(ZSTD_window_t* window, const void* blockEnd, U32 maxDist, U32* loadedDictEndPtr, const ZSTD_matchState_t** dictMatchStatePtr) {`


ZSTD_window_enforceMaxDist():
Updates lowLimit so that:
(srcEnd - base) - lowLimit == maxDist + loadedDictEnd

It ensures index is valid as long as index >= lowLimit.
This must be called before a block compression call.

loadedDictEnd is only defined if a dictionary is in use for current compression.
As the name implies, loadedDictEnd represents the index at end of dictionary.
The value lies within context's referential, it can be directly compared to blockEndIdx.

If loadedDictEndPtr is NULL, no dictionary is in use, and we use loadedDictEnd == 0.
If loadedDictEndPtr is not NULL, we set it to zero after updating lowLimit.
This is because dictionaries are allowed to be referenced fully
as long as the last byte of the dictionary is in the window.
Once input has progressed beyond window size, dictionary cannot be referenced anymore.

In normal dict mode, the dictionary lies between lowLimit and dictLimit.
In dictMatchState mode, lowLimit and dictLimit are the same,
and the dictionary is below them.
forceWindow and dictMatchState are therefore incompatible.


---

## `MEM_STATIC U32 ZSTD_window_update(ZSTD_window_t* window, void const* src, size_t srcSize, int forceNonContiguous) {`


ZSTD_window_update():
Updates the window by appending [src, src + srcSize) to the window.
If it is not contiguous, the current prefix becomes the extDict, and we
forget about the extDict. Handles overlap of the prefix and extDict.
Returns non-zero if the segment is contiguous.


---

## `MEM_STATIC U32 ZSTD_getLowestMatchIndex(const ZSTD_matchState_t* ms, U32 curr, unsigned windowLog) {`


Returns the lowest allowed match index. It may either be in the ext-dict or the prefix.


---

## `MEM_STATIC U32 ZSTD_getLowestPrefixIndex(const ZSTD_matchState_t* ms, U32 curr, unsigned windowLog) {`


Returns the lowest allowed match index in the prefix.


---

## `U32 ZSTD_cycleLog(U32 hashLog, ZSTD_strategy strat);`

ZSTD_cycleLog() :
condition for correct operation : hashLog > 1 

---

## `void ZSTD_CCtx_trace(ZSTD_CCtx* cctx, size_t extraCSize);`

ZSTD_CCtx_trace() :
Trace the end of a compression call.


---

