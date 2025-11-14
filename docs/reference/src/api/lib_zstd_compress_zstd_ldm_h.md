# lib/zstd/compress/zstd_ldm.h

## `size_t ZSTD_ldm_generateSequences( ldmState_t* ldms, rawSeqStore_t* sequences, ldmParams_t const* params, void const* src, size_t srcSize);`


ZSTD_ldm_generateSequences():

Generates the sequences using the long distance match finder.
Generates long range matching sequences in `sequences`, which parse a prefix
of the source. `sequences` must be large enough to store every sequence,
which can be checked with `ZSTD_ldm_getMaxNbSeq()`.

- **Returns**: s 0 or an error code.

NOTE: The user must have called ZSTD_window_update() for all of the input
they have, even if they pass it to ZSTD_ldm_generateSequences() in chunks.
NOTE: This function returns an error if it runs out of space to store
sequences.


---

## `size_t ZSTD_ldm_blockCompress(rawSeqStore_t* rawSeqStore, ZSTD_matchState_t* ms, seqStore_t* seqStore, U32 rep[ZSTD_REP_NUM], ZSTD_paramSwitch_e useRowMatchFinder, void const* src, size_t srcSize);`


ZSTD_ldm_blockCompress():

Compresses a block using the predefined sequences, along with a secondary
block compressor. The literals section of every sequence is passed to the
secondary block compressor, and those sequences are interspersed with the
predefined sequences. Returns the length of the last literals.
Updates `rawSeqStore.pos` to indicate how many sequences have been consumed.
`rawSeqStore.seq` may also be updated to split the last sequence between two
blocks.

- **Returns**: The length of the last literals.

NOTE: The source must be at most the maximum block size, but the predefined
sequences can be any size, and may be longer than the block. In the case that
they are longer than the block, the last sequences may need to be split into
two. We handle that case correctly, and update `rawSeqStore` appropriately.
NOTE: This function does not return any errors.


---

## `void ZSTD_ldm_skipSequences(rawSeqStore_t* rawSeqStore, size_t srcSize, U32 const minMatch);`


ZSTD_ldm_skipSequences():

Skip past `srcSize` bytes worth of sequences in `rawSeqStore`.
Avoids emitting matches less than `minMatch` bytes.
Must be called for data that is not passed to ZSTD_ldm_blockCompress().


---

## `size_t ZSTD_ldm_getTableSize(ldmParams_t params);`

ZSTD_ldm_getTableSize() :
Estimate the space needed for long distance matching tables or 0 if LDM is
disabled.


---

## `size_t ZSTD_ldm_getMaxNbSeq(ldmParams_t params, size_t maxChunkSize);`

ZSTD_ldm_getSeqSpace() :
Return an upper bound on the number of sequences that can be produced by
the long distance matcher, or 0 if LDM is disabled.


---

## `void ZSTD_ldm_adjustParameters(ldmParams_t* params, ZSTD_compressionParameters const* cParams);`

ZSTD_ldm_adjustParameters() :
If the params->hashRateLog is not set, set it to its default value based on
windowLog and params->hashLog.

Ensures that params->bucketSizeLog is <= params->hashLog (setting it to
params->hashLog if it is not).

Ensures that the minMatchLength >= targetLength during optimal parsing.


---

