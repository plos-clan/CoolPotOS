# lib/zstd/decompress/zstd_decompress.c

## `#define DDICT_HASHSET_MAX_LOAD_FACTOR_COUNT_MULT 4 #define DDICT_HASHSET_MAX_LOAD_FACTOR_SIZE_MULT \ 3 /* These two constants represent SIZE_MULT/COUNT_MULT load factor without using a float. * Currently, that means a 0.75 load factor. * So, if count * COUNT_MULT / size * SIZE_MULT != 0, then we've exceeded * the load factor of the ddict hash set. */ #define DDICT_HASHSET_TABLE_BASE_SIZE 64 #define DDICT_HASHSET_RESIZE_FACTOR 2 /* Hash function to determine starting position of dict insertion within the table * Returns an index between [0, hashSet->ddictPtrTableSize] */ static size_t ZSTD_DDictHashSet_getIndex(const ZSTD_DDictHashSet *hashSet, U32 dictID) {`

**********************************
Multiple DDicts Hashset internals *
***********************************

---

## `static size_t ZSTD_frameHeaderSize_internal(const void *src, size_t srcSize, ZSTD_format_e format) {`

ZSTD_frameHeaderSize_internal() :
srcSize must be large enough to reach header size fields.
note : only works for formats ZSTD_f_zstd1 and ZSTD_f_zstd1_magicless.

- **Returns**: : size of the Frame Header
or an error code, which can be tested with ZSTD_isError() 

---

## `size_t ZSTD_frameHeaderSize(const void *src, size_t srcSize) {`

ZSTD_frameHeaderSize() :
srcSize must be >= ZSTD_frameHeaderSize_prefix.

- **Returns**: : size of the Frame Header,
or an error code (if srcSize is too small) 

---

## `size_t ZSTD_getFrameHeader_advanced(ZSTD_frameHeader *zfhPtr, const void *src, size_t srcSize, ZSTD_format_e format) {`

ZSTD_getFrameHeader_advanced() :
decode Frame Header, or require larger `srcSize`.
note : only works for formats ZSTD_f_zstd1 and ZSTD_f_zstd1_magicless

- **Returns**: : 0, `zfhPtr` is correctly filled,
>0, `srcSize` is too small, value is wanted `srcSize` amount,
or an error code, which can be tested using ZSTD_isError() 

---

## `size_t ZSTD_getFrameHeader(ZSTD_frameHeader *zfhPtr, const void *src, size_t srcSize) {`

ZSTD_getFrameHeader() :
decode Frame Header, or require larger `srcSize`.
note : this function does not consume input, it only reads it.

- **Returns**: : 0, `zfhPtr` is correctly filled,
>0, `srcSize` is too small, value is wanted `srcSize` amount,
or an error code, which can be tested using ZSTD_isError() 

---

## `unsigned long long ZSTD_getFrameContentSize(const void *src, size_t srcSize) {`

ZSTD_getFrameContentSize() :
compatible with legacy mode

- **Returns**: : decompressed size of the single frame pointed to be `src` if known, otherwise
- ZSTD_CONTENTSIZE_UNKNOWN if the size cannot be determined
- ZSTD_CONTENTSIZE_ERROR if an error occurred (e.g. invalid magic number, srcSize too small) 

---

## `unsigned long long ZSTD_findDecompressedSize(const void *src, size_t srcSize) {`

ZSTD_findDecompressedSize() :
compatible with legacy mode
`srcSize` must be the exact length of some number of ZSTD compressed and/or
skippable frames

- **Returns**: : decompressed size of the frames contained 

---

## `unsigned long long ZSTD_getDecompressedSize(const void *src, size_t srcSize) {`

ZSTD_getDecompressedSize() :
compatible with legacy mode

- **Returns**: : decompressed size if known, 0 otherwise
note : 0 can mean any of the following :
- frame content is empty
- decompressed size field is not present in frame header
- frame header unknown / not supported
- frame header not complete (`srcSize` too small) 

---

## `static size_t ZSTD_decodeFrameHeader(ZSTD_DCtx *dctx, const void *src, size_t headerSize) {`

ZSTD_decodeFrameHeader() :
`headerSize` must be the size provided by ZSTD_frameHeaderSize().
If multiple DDict references are enabled, also will choose the correct DDict to use.

- **Returns**: : 0 if success, or an error code, which can be tested using ZSTD_isError() 

---

## `size_t ZSTD_findFrameCompressedSize(const void *src, size_t srcSize) {`

ZSTD_findFrameCompressedSize() :
compatible with legacy mode
`src` must point to the start of a ZSTD frame, ZSTD legacy frame, or skippable frame
`srcSize` must be at least as large as the frame contained

- **Returns**: : the compressed size of the frame starting at `src` 

---

## `unsigned long long ZSTD_decompressBound(const void *src, size_t srcSize) {`

ZSTD_decompressBound() :
compatible with legacy mode
`src` must point to the start of a ZSTD frame or a skippeable frame
`srcSize` must be at least as large as the frame contained

- **Returns**: : the maximum decompressed size of the compressed source


---

## `size_t ZSTD_insertBlock(ZSTD_DCtx *dctx, const void *blockStart, size_t blockSize) {`

ZSTD_insertBlock() :
insert `src` block into `dctx` history. Useful to track uncompressed blocks. 

---

## `static size_t ZSTD_nextSrcSizeToDecompressWithInputSize(ZSTD_DCtx *dctx, size_t inputSize) {`


Similar to ZSTD_nextSrcSizeToDecompress(), but when when a block input can be streamed,
we allow taking a partial block as the input. Currently only raw uncompressed blocks can
be streamed.

For blocks that can be streamed, this allows us to reduce the latency until we produce
output, and avoid copying the input.


- **`inputSize`**: - The total amount of input that the caller currently has.


---

## `size_t ZSTD_decompressContinue(ZSTD_DCtx *dctx, void *dst, size_t dstCapacity, const void *src, size_t srcSize) {`

ZSTD_decompressContinue() :
srcSize : must be the exact nb of bytes expected (see ZSTD_nextSrcSizeToDecompress())

- **Returns**: : nb of bytes generated into `dst` (necessarily <= `dstCapacity)
or an error code, which can be tested using ZSTD_isError() 

---

