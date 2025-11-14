# lib/zstd/common/huf.h

## `HUF_PUBLIC_API size_t HUF_compress(void* dst, size_t dstCapacity, const void* src, size_t srcSize);`

HUF_compress() :
Compress content from buffer 'src', of size 'srcSize', into buffer 'dst'.
'dst' buffer must be already allocated.
Compression runs faster if `dstCapacity` >= HUF_compressBound(srcSize).
`srcSize` must be <= `HUF_BLOCKSIZE_MAX` == 128 KB.

- **Returns**: : size of compressed data (<= `dstCapacity`).
Special values : if return == 0, srcData is not compressible => Nothing is stored within dst !!!
if HUF_isError(return), compression failed (more details using HUF_getErrorName())


---

## `HUF_PUBLIC_API size_t HUF_decompress(void* dst, size_t originalSize, const void* cSrc, size_t cSrcSize);`

HUF_decompress() :
Decompress HUF data from buffer 'cSrc', of size 'cSrcSize',
into already allocated buffer 'dst', of minimum size 'dstSize'.
`originalSize` : **must** be the ***exact*** size of original (uncompressed) data.
Note : in contrast with FSE, HUF_decompress can regenerate
RLE (cSrcSize==1) and uncompressed (cSrcSize==dstSize) data,
because it knows size to regenerate (originalSize).

- **Returns**: : size of regenerated data (== originalSize),
or an error code, which can be tested using HUF_isError()


---

## `HUF_PUBLIC_API size_t HUF_compressBound(size_t size);`

< maximum input size for a single block compressed with HUF_compress 

---

## `/* Error Management */ HUF_PUBLIC_API unsigned HUF_isError(size_t code);`

< maximum compressed size (worst case) 

---

## `HUF_PUBLIC_API const char* HUF_getErrorName(size_t code);`

< tells if a return value is an error code 

---

## `HUF_PUBLIC_API size_t HUF_compress2 (void* dst, size_t dstCapacity, const void* src, size_t srcSize, unsigned maxSymbolValue, unsigned tableLog);`

HUF_compress2() :
Same as HUF_compress(), but offers control over `maxSymbolValue` and `tableLog`.
`maxSymbolValue` must be <= HUF_SYMBOLVALUE_MAX .
`tableLog` must be `<= HUF_TABLELOG_MAX` . 

---

## `#define HUF_WORKSPACE_SIZE ((8 << 10) + 512 /* sorting scratch space */) #define HUF_WORKSPACE_SIZE_U64 (HUF_WORKSPACE_SIZE / sizeof(U64)) HUF_PUBLIC_API size_t HUF_compress4X_wksp (void* dst, size_t dstCapacity, const void* src, size_t srcSize, unsigned maxSymbolValue, unsigned tableLog, void* workSpace, size_t wkspSize);`

HUF_compress4X_wksp() :
Same as HUF_compress2(), but uses externally allocated `workSpace`.
`workspace` must be at least as large as HUF_WORKSPACE_SIZE 

---

## `#ifndef HUF_FORCE_DECOMPRESS_X1 size_t HUF_decompress4X2 (void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);`

< single-symbol decoder 

---

## `#endif size_t HUF_decompress4X_DCtx (HUF_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);`

< double-symbols decoder 

---

## `size_t HUF_decompress4X_hufOnly(HUF_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);`

< decodes RLE and uncompressed 

---

## `size_t HUF_decompress4X_hufOnly_wksp(HUF_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize, void* workSpace, size_t wkspSize);`

< considers RLE and uncompressed as errors 

---

## `size_t HUF_decompress4X1_DCtx(HUF_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);`

< considers RLE and uncompressed as errors 

---

## `size_t HUF_decompress4X1_DCtx_wksp(HUF_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize, void* workSpace, size_t wkspSize);`

< single-symbol decoder 

---

## `#ifndef HUF_FORCE_DECOMPRESS_X1 size_t HUF_decompress4X2_DCtx(HUF_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);`

< single-symbol decoder 

---

## `size_t HUF_decompress4X2_DCtx_wksp(HUF_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize, void* workSpace, size_t wkspSize);`

< double-symbols decoder 

---

## `#endif /* **************************************** * HUF detailed API * ****************************************/ /*! HUF_compress() does the following: * 1. count symbol occurrence from source[] into table count[] using FSE_count() (exposed within "fse.h") * 2. (optional) refine tableLog using HUF_optimalTableLog() * 3. build Huffman table from count using HUF_buildCTable() * 4. save Huffman table to memory buffer using HUF_writeCTable() * 5. encode the data stream using HUF_compress4X_usingCTable() * * The following API allows targeting specific sub-functions for advanced tasks. * For example, it's possible to compress several blocks using the same 'CTable', * or to save and regenerate 'CTable' using external methods. */ unsigned HUF_optimalTableLog(unsigned maxTableLog, size_t srcSize, unsigned maxSymbolValue);`

< double-symbols decoder 

---

## `} HUF_repeat;`

< Can use the previous table and it is assumed to be valid 

---

## `HUF_CElt* hufTable, HUF_repeat* repeat, int preferRepeat, int bmi2, unsigned suspectUncompressible);`

< `workSpace` must be aligned on 4-bytes boundaries, `wkspSize` must be >= HUF_WORKSPACE_SIZE 

---

## `#define HUF_CTABLE_WORKSPACE_SIZE_U32 (2*HUF_SYMBOLVALUE_MAX +1 +1) #define HUF_CTABLE_WORKSPACE_SIZE (HUF_CTABLE_WORKSPACE_SIZE_U32 * sizeof(unsigned)) size_t HUF_buildCTable_wksp (HUF_CElt* tree, const unsigned* count, U32 maxSymbolValue, U32 maxNbBits, void* workSpace, size_t wkspSize);`

HUF_buildCTable_wksp() :
Same as HUF_buildCTable(), but using externally allocated scratch buffer.
`workSpace` must be aligned on 4-bytes boundaries, and its size must be >= HUF_CTABLE_WORKSPACE_SIZE.


---

## `size_t HUF_readCTable (HUF_CElt* CTable, unsigned* maxSymbolValuePtr, const void* src, size_t srcSize, unsigned *hasZeroWeights);`

HUF_readCTable() :
Loading a CTable saved with HUF_writeCTable() 

---

## `U32 HUF_getNbBitsFromCTable(const HUF_CElt* symbolTable, U32 symbolValue);`

HUF_getNbBitsFromCTable() :
Read nbBits from CTable symbolTable, for symbol `symbolValue` presumed <= HUF_SYMBOLVALUE_MAX
Note 1 : is not inlined, as HUF_CElt definition is private 

---

## `U32 HUF_selectDecoder (size_t dstSize, size_t cSrcSize);`

HUF_selectDecoder() :
Tells which decoder is likely to decode faster,
based on a set of pre-computed metrics.

- **Returns**: : 0==HUF_decompress4X1, 1==HUF_decompress4X2 .
Assumption : 0 < dstSize <= 128 KB 

---

## `#define HUF_DECOMPRESS_WORKSPACE_SIZE ((2 << 10) + (1 << 9)) #define HUF_DECOMPRESS_WORKSPACE_SIZE_U32 (HUF_DECOMPRESS_WORKSPACE_SIZE / sizeof(U32)) #ifndef HUF_FORCE_DECOMPRESS_X2 size_t HUF_readDTableX1 (HUF_DTable* DTable, const void* src, size_t srcSize);`


The minimum workspace size for the `workSpace` used in
HUF_readDTableX1_wksp() and HUF_readDTableX2_wksp().

The space used depends on HUF_TABLELOG_MAX, ranging from ~1500 bytes when
HUF_TABLE_LOG_MAX=12 to ~1850 bytes when HUF_TABLE_LOG_MAX=15.
Buffer overflow errors may potentially occur if code modifications result in
a required workspace size greater than that specified in the following
macro.


---

## `size_t HUF_compress1X_usingCTable(void* dst, size_t dstSize, const void* src, size_t srcSize, const HUF_CElt* CTable);`

< `workSpace` must be a table of at least HUF_WORKSPACE_SIZE_U64 U64 

---

## `HUF_CElt* hufTable, HUF_repeat* repeat, int preferRepeat, int bmi2, unsigned suspectUncompressible);`

< `workSpace` must be aligned on 4-bytes boundaries, `wkspSize` must be >= HUF_WORKSPACE_SIZE 

---

## `size_t HUF_decompress1X1_DCtx_wksp(HUF_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize, void* workSpace, size_t wkspSize);`

< single-symbol decoder 

---

## `#endif #ifndef HUF_FORCE_DECOMPRESS_X1 size_t HUF_decompress1X2_DCtx(HUF_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);`

< single-symbol decoder 

---

## `size_t HUF_decompress1X2_DCtx_wksp(HUF_DTable* dctx, void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize, void* workSpace, size_t wkspSize);`

< double-symbols decoder 

---

## `#endif size_t HUF_decompress1X_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUF_DTable* DTable);`

< double-symbols decoder 

---

## `#ifndef HUF_FORCE_DECOMPRESS_X2 size_t HUF_decompress1X1_usingDTable(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const HUF_DTable* DTable);`

< automatic selection of sing or double symbol decoder, based on DTable 

---

