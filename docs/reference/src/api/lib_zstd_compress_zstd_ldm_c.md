# lib/zstd/compress/zstd_ldm.c

## `static void ZSTD_ldm_gear_init(ldmRollingHashState_t *state, ldmParams_t const *params) {`

ZSTD_ldm_gear_init():

Initializes the rolling hash state such that it will honor the
settings in params. 

---

## `static void ZSTD_ldm_gear_reset(ldmRollingHashState_t *state, BYTE const *data, size_t minMatchLength) {`

ZSTD_ldm_gear_reset()
Feeds [data, data + minMatchLength) into the hash without registering any
splits. This effectively resets the hash state. This is used when skipping
over data, either at the beginning of a block, or skipping sections.


---

## `static size_t ZSTD_ldm_gear_feed(ldmRollingHashState_t *state, BYTE const *data, size_t size, size_t *splits, unsigned *numSplits) {`

ZSTD_ldm_gear_feed():

Registers in the splits array all the split points found in the first
size bytes following the data pointer. This function terminates when
either all the data has been processed or LDM_BATCH_SIZE splits are
present in the splits array.

Precondition: The splits array must not be full.
Returns: The number of bytes processed. 

---

## `static ldmEntry_t *ZSTD_ldm_getBucket(ldmState_t *ldmState, size_t hash, ldmParams_t const ldmParams) {`

ZSTD_ldm_getBucket() :
Returns a pointer to the start of the bucket associated with hash. 

---

## `static void ZSTD_ldm_insertEntry(ldmState_t *ldmState, size_t const hash, const ldmEntry_t entry, ldmParams_t const ldmParams) {`

ZSTD_ldm_insertEntry() :
Insert the entry with corresponding hash into the hash table 

---

## `static size_t ZSTD_ldm_countBackwardsMatch(const BYTE *pIn, const BYTE *pAnchor, const BYTE *pMatch, const BYTE *pMatchBase) {`

ZSTD_ldm_countBackwardsMatch() :
Returns the number of bytes that match backwards before pIn and pMatch.

We count only bytes where pMatch >= pBase and pIn >= pAnchor. 

---

## `static size_t ZSTD_ldm_countBackwardsMatch_2segments(const BYTE *pIn, const BYTE *pAnchor, const BYTE *pMatch, const BYTE *pMatchBase, const BYTE *pExtDictStart, const BYTE *pExtDictEnd) {`

ZSTD_ldm_countBackwardsMatch_2segments() :
Returns the number of bytes that match backwards from pMatch,
even with the backwards match spanning 2 different segments.

On reaching `pMatchBase`, start counting from mEnd 

---

## `static size_t ZSTD_ldm_fillFastTables(ZSTD_matchState_t *ms, void const *end) {`

ZSTD_ldm_fillFastTables() :

Fills the relevant tables for the ZSTD_fast and ZSTD_dfast strategies.
This is similar to ZSTD_loadDictionaryContent.

The tables for the other strategies are filled within their
block compressors. 

---

## `static void ZSTD_ldm_limitTableUpdate(ZSTD_matchState_t *ms, const BYTE *anchor) {`

ZSTD_ldm_limitTableUpdate() :

Sets cctx->nextToUpdate to a position corresponding closer to anchor
if it is far way
(after a long match, only update tables a limited amount). 

---

## `static rawSeq maybeSplitSequence(rawSeqStore_t *rawSeqStore, U32 const remaining, U32 const minMatch) {`


If the sequence length is longer than remaining then the sequence is split
between this block and the next.

Returns the current sequence to handle, or if the rest of the block should
be literals, it returns a sequence with offset == 0.


---

