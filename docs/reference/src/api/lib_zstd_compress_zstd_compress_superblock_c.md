# lib/zstd/compress/zstd_compress_superblock.c

## `static size_t ZSTD_compressSubBlock_literal(const HUF_CElt *hufTable, const ZSTD_hufCTablesMetadata_t *hufMetadata, const BYTE *literals, size_t litSize, void *dst, size_t dstSize, const int bmi2, int writeEntropy, int *entropyWritten) {`

ZSTD_compressSubBlock_literal() :
Compresses literals section for a sub-block.
When we have to write the Huffman table we will sometimes choose a header
size larger than necessary. This is because we have to pick the header size
before we know the table size + compressed size, so we have a bound on the
table size. If we guessed incorrectly, we fall back to uncompressed literals.

We write the header when writeEntropy=1 and set entropyWritten=1 when we succeeded
in writing the header, otherwise it is set to 0.

hufMetadata->hType has literals block type info.
If it is set_basic, all sub-blocks literals section will be Raw_Literals_Block.
If it is set_rle, all sub-blocks literals section will be RLE_Literals_Block.
If it is set_compressed, first sub-block's literals section will be Compressed_Literals_Block
If it is set_compressed, first sub-block's literals section will be Treeless_Literals_Block
and the following sub-blocks' literals sections will be Treeless_Literals_Block.

- **Returns**: : compressed size of literals section of a sub-block
Or 0 if it unable to compress.
Or error code 

---

## `static size_t ZSTD_compressSubBlock_sequences( const ZSTD_fseCTables_t *fseTables, const ZSTD_fseCTablesMetadata_t *fseMetadata, const seqDef *sequences, size_t nbSeq, const BYTE *llCode, const BYTE *mlCode, const BYTE *ofCode, const ZSTD_CCtx_params *cctxParams, void *dst, size_t dstCapacity, const int bmi2, int writeEntropy, int *entropyWritten) {`

ZSTD_compressSubBlock_sequences() :
Compresses sequences section for a sub-block.
fseMetadata->llType, fseMetadata->ofType, and fseMetadata->mlType have
symbol compression modes for the super-block.
The first successfully compressed block will have these in its header.
We set entropyWritten=1 when we succeed in compressing the sequences.
The following sub-blocks will always have repeat mode.

- **Returns**: : compressed size of sequences section of a sub-block
Or 0 if it is unable to compress
Or error code. 

---

## `static size_t ZSTD_compressSubBlock(const ZSTD_entropyCTables_t *entropy, const ZSTD_entropyCTablesMetadata_t *entropyMetadata, const seqDef *sequences, size_t nbSeq, const BYTE *literals, size_t litSize, const BYTE *llCode, const BYTE *mlCode, const BYTE *ofCode, const ZSTD_CCtx_params *cctxParams, void *dst, size_t dstCapacity, const int bmi2, int writeLitEntropy, int writeSeqEntropy, int *litEntropyWritten, int *seqEntropyWritten, U32 lastBlock) {`

ZSTD_compressSubBlock() :
Compresses a single sub-block.

- **Returns**: : compressed size of the sub-block
Or 0 if it failed to compress. 

---

## `static size_t ZSTD_compressSubBlock_multi( const seqStore_t *seqStorePtr, const ZSTD_compressedBlockState_t *prevCBlock, ZSTD_compressedBlockState_t *nextCBlock, const ZSTD_entropyCTablesMetadata_t *entropyMetadata, const ZSTD_CCtx_params *cctxParams, void *dst, size_t dstCapacity, const void *src, size_t srcSize, const int bmi2, U32 lastBlock, void *workspace, size_t wkspSize) {`

ZSTD_compressSubBlock_multi() :
Breaks super-block into multiple sub-blocks and compresses them.
Entropy will be written to the first block.
The following blocks will use repeat mode to compress.
All sub-blocks are compressed blocks (no raw or rle blocks).

- **Returns**: : compressed size of the super block (which is multiple ZSTD blocks)
Or 0 if it failed to compress. 

---

