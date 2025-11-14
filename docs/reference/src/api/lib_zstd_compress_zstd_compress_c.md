# lib/zstd/compress/zstd_compress.c

## `static void ZSTD_clearAllDicts(ZSTD_CCtx *cctx) {`


Clears and frees all of the dictionaries in the CCtx.


---

## `static void ZSTD_CCtxParams_init_internal(ZSTD_CCtx_params *cctxParams, ZSTD_parameters const *params, int compressionLevel) {`


Initializes the cctxParams from params and compressionLevel.

- **`compressionLevel`**: If params are derived from a compression level then that compression level, otherwise ZSTD_NO_CLEVEL.


---

## `static void ZSTD_CCtxParams_setZstdParams(ZSTD_CCtx_params *cctxParams, const ZSTD_parameters *params) {`


Sets cctxParams' cParams and fParams from params, but otherwise leaves them alone.

- **`param`**: Validated zstd parameters.


---

## `size_t ZSTD_CCtx_setParametersUsingCCtxParams(ZSTD_CCtx *cctx, const ZSTD_CCtx_params *params) {`

ZSTD_CCtx_setParametersUsingCCtxParams() :
just applies `params` into `cctx`
no action is performed, parameters are merely stored.
If ZSTDMT is enabled, parameters are pushed to cctx->mtctx.
This is possible even if a compression is ongoing.
In which case, new parameters will be applied on the fly, starting with next compression job.


---

## `static size_t ZSTD_initLocalDict(ZSTD_CCtx *cctx) {`


Initializes the local dict using the requested parameters.
NOTE: This does not use the pledged src size, because it may be used for more
than one compression.


---

## `size_t ZSTD_checkCParams(ZSTD_compressionParameters cParams) {`

ZSTD_checkCParams() :
control CParam values remain within authorized range.

- **Returns**: : 0, or an error code if one value is beyond authorized range 

---

## `static ZSTD_compressionParameters ZSTD_clampCParams(ZSTD_compressionParameters cParams) {`

ZSTD_clampCParams() :
make CParam values within valid range.

- **Returns**: : valid CParams 

---

## `U32 ZSTD_cycleLog(U32 hashLog, ZSTD_strategy strat) {`

ZSTD_cycleLog() :
condition for correct operation : hashLog > 1 

---

## `static U32 ZSTD_dictAndWindowLog(U32 windowLog, U64 srcSize, U64 dictSize) {`

ZSTD_dictAndWindowLog() :
Returns an adjusted window log that is large enough to fit the source and the dictionary.
The zstd format says that the entire dictionary is valid if one byte of the dictionary
is within the window. So the hashLog and chainLog should be large enough to reference both
the dictionary and the window. So we must use this adjusted dictAndWindowLog when downsizing
the hashLog and windowLog.
NOTE: srcSize must not be ZSTD_CONTENTSIZE_UNKNOWN.


---

## `static ZSTD_compressionParameters ZSTD_adjustCParams_internal(ZSTD_compressionParameters cPar, unsigned long long srcSize, size_t dictSize, ZSTD_cParamMode_e mode) {`

ZSTD_adjustCParams_internal() :
optimize `cPar` for a specified input (`srcSize` and `dictSize`).
mostly downsize to reduce memory consumption and initialization latency.
`srcSize` can be ZSTD_CONTENTSIZE_UNKNOWN when not known.
`mode` is the mode for parameter adjustment. See docs for `ZSTD_cParamMode_e`.
note : `srcSize==0` means 0!
condition : cPar is presumed validated (can be checked using ZSTD_checkCParams()). 

---

## `typedef enum {`


Controls, for this matchState reset, whether the tables need to be cleared /
prepared for the coming compression (ZSTDcrp_makeClean), or whether the
tables can be left unclean (ZSTDcrp_leaveDirty), because we know that a
subsequent operation will overwrite the table space anyways (e.g., copying
the matchState contents in from a CDict).


---

## `typedef enum {`


Controls, for this matchState reset, whether indexing can continue where it
left off (ZSTDirp_continue), or whether it needs to be restarted from zero
(ZSTDirp_reset).


---

## `static int ZSTD_dictTooBig(size_t const loadedDictSize) {`

ZSTD_dictTooBig():
When dictionaries are larger than ZSTD_CHUNKSIZE_MAX they can't be loaded in
one go generically. So we ensure that in that case we reset the tables to zero,
so that we can load as much of the dictionary as possible.


---

## `static size_t ZSTD_buildBlockEntropyStats_literals(void *const src, size_t srcSize, const ZSTD_hufCTables_t *prevHuf, ZSTD_hufCTables_t *nextHuf, ZSTD_hufCTablesMetadata_t *hufMetadata, const int literalsCompressionIsDisabled, void *workspace, size_t wkspSize) {`

ZSTD_buildBlockEntropyStats_literals() :
Builds entropy for the literals.
Stores literals block type (raw, rle, compressed, repeat) and
huffman description table to hufMetadata.
Requires ENTROPY_WORKSPACE_SIZE workspace

- **Returns**: : size of huffman description table or error code 

---

## `static size_t ZSTD_buildBlockEntropyStats_sequences(seqStore_t *seqStorePtr, const ZSTD_fseCTables_t *prevEntropy, ZSTD_fseCTables_t *nextEntropy, const ZSTD_CCtx_params *cctxParams, ZSTD_fseCTablesMetadata_t *fseMetadata, void *workspace, size_t wkspSize) {`

ZSTD_buildBlockEntropyStats_sequences() :
Builds entropy for the sequences.
Stores symbol compression modes and fse table to fseMetadata.
Requires ENTROPY_WORKSPACE_SIZE wksp.

- **Returns**: : size of fse tables or error code 

---

## `size_t ZSTD_buildBlockEntropyStats(seqStore_t *seqStorePtr, const ZSTD_entropyCTables_t *prevEntropy, ZSTD_entropyCTables_t *nextEntropy, const ZSTD_CCtx_params *cctxParams, ZSTD_entropyCTablesMetadata_t *entropyMetadata, void *workspace, size_t wkspSize) {`

ZSTD_buildBlockEntropyStats() :
Builds entropy for the block.
Requires workspace size ENTROPY_WORKSPACE_SIZE


- **Returns**: : 0 on success or error code


---

## `static U32 ZSTD_resolveRepcodeToRawOffset(const U32 rep[ZSTD_REP_NUM], const U32 offCode, const U32 ll0) {`


Returns the raw offset represented by the combination of offCode, ll0, and repcode history.
offCode must represent a repcode in the numeric representation of ZSTD_storeSeq().


---

## `static void ZSTD_seqStore_resolveOffCodes(repcodes_t *const dRepcodes, repcodes_t *const cRepcodes, seqStore_t *const seqStore, U32 const nbSeq) {`


ZSTD_seqStore_resolveOffCodes() reconciles any possible divergences in offset history that may arise
due to emission of RLE/raw blocks that disturb the offset history,
and replaces any repcodes within the seqStore that may be invalid.

dRepcodes are updated as would be on the decompression side.
cRepcodes are updated exactly in accordance with the seqStore.

Note : this function assumes seq->offBase respects the following numbering scheme :
0 : invalid
1-3 : repcode 1-3
4+ : real_offset+3


---

## `static size_t ZSTD_compress_insertDictionary(ZSTD_compressedBlockState_t *bs, ZSTD_matchState_t *ms, ldmState_t *ls, ZSTD_cwksp *ws, const ZSTD_CCtx_params *params, const void *dict, size_t dictSize, ZSTD_dictContentType_e dictContentType, ZSTD_dictTableLoadMethod_e dtlm, void *workspace) {`

ZSTD_compress_insertDictionary() :

- **Returns**: : dictID, or an error code 

---

## `static size_t ZSTD_compressStream_generic(ZSTD_CStream *zcs, ZSTD_outBuffer *output, ZSTD_inBuffer *input, ZSTD_EndDirective const flushMode) {`

ZSTD_compressStream_generic():
internal function for all *compressStream*() variants
non-static, because can be called from zstdmt_compress.c

- **Returns**: : hint size for next input 

---

## `static void ZSTD_dedicatedDictSearch_revertCParams(ZSTD_compressionParameters *cParams) {`


Reverses the adjustment applied to cparams when enabling dedicated dict
search. This is used to recover the params set to be used in the working
context. (Otherwise, those tables would also grow.)


---

