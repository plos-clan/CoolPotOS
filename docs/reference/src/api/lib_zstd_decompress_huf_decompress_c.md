# lib/zstd/decompress/huf_decompress.c

## `static size_t HUF_DecompressAsmArgs_init(HUF_DecompressAsmArgs* args, void* dst, size_t dstSize, void const* src, size_t srcSize, const HUF_DTable* DTable) {`


Initializes args for the asm decoding loop.

- **Returns**: s 0 on success
1 if the fallback implementation should be used.
Or an error code on failure.


---

## `static U64 HUF_DEltX1_set4(BYTE symbol, BYTE nbBits) {`


Packs 4 HUF_DEltX1 structs into a U64. This is used to lay down 4 entries at
a time.


---

## `static U32 HUF_rescaleStats(BYTE* huffWeight, U32* rankVal, U32 nbSymbols, U32 tableLog, U32 targetTableLog) {`


Increase the tableLog to targetTableLog and rescales the stats.
If tableLog > targetTableLog this is a no-op.

- **Returns**: s New tableLog


---

## `static U32 HUF_buildDEltX2U32(U32 symbol, U32 nbBits, U32 baseSeq, int level) {`


Constructs a HUF_DEltX2 in a U32.


---

## `static HUF_DEltX2 HUF_buildDEltX2(U32 symbol, U32 nbBits, U32 baseSeq, int level) {`


Constructs a HUF_DEltX2.


---

## `static U64 HUF_buildDEltX2U64(U32 symbol, U32 nbBits, U16 baseSeq, int level) {`


Constructs 2 HUF_DEltX2s and packs them into a U64.


---

## `static void HUF_fillDTableX2ForWeight( HUF_DEltX2* DTableRank, sortedSymbol_t const* begin, sortedSymbol_t const* end, U32 nbBits, U32 tableLog, U16 baseSeq, int const level) {`


Fills the DTable rank with all the symbols from [begin, end) that are each
nbBits long.


- **`DTableRank`**: The start of the rank in the DTable.
- **`begin`**: The first symbol to fill (inclusive).
- **`end`**: The last symbol to fill (exclusive).
- **`nbBits`**: Each symbol is nbBits long.
- **`tableLog`**: The table log.
- **`baseSeq`**: If level == 1 { 0 } else { the first level symbol }
- **`level`**: The level in the table. Must be 1 or 2.


---

## `U32 HUF_selectDecoder (size_t dstSize, size_t cSrcSize) {`

HUF_selectDecoder() :
Tells which decoder is likely to decode faster,
based on a set of pre-computed metrics.

- **Returns**: : 0==HUF_decompress4X1, 1==HUF_decompress4X2 .
Assumption : 0 < dstSize <= 128 KB 

---

