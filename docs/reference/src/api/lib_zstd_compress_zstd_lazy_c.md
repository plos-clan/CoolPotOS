# lib/zstd/compress/zstd_lazy.c

## `static void ZSTD_insertDUBT1(const ZSTD_matchState_t *ms, U32 curr, const BYTE *inputEnd, U32 nbCompares, U32 btLow, const ZSTD_dictMode_e dictMode) {`

ZSTD_insertDUBT1() :
sort one already inserted but unsorted position
assumption : curr >= btlow == (curr - btmask)
doesn't fail 

---

## `FORCE_INLINE_TEMPLATE size_t ZSTD_BtFindBestMatch(ZSTD_matchState_t *ms, const BYTE *const ip, const BYTE *const iLimit, size_t *offsetPtr, const U32 mls /* template */, const ZSTD_dictMode_e dictMode) {`

ZSTD_BtFindBestMatch() : Tree updater, providing best match 

---

## `void ZSTD_dedicatedDictSearch_lazy_loadDictionary(ZSTD_matchState_t *ms, const BYTE *const ip) {`

********************************
Dedicated dict search
*********************************

---

## `typedef struct {`


This struct contains the functions necessary for lazy to search.
Currently, that is only searchMax. However, it is still valuable to have the
VTable because this makes it easier to add more functions to the VTable later.

TODO: The start of the search function involves loading and calculating a
bunch of constants from the ZSTD_matchState_t. These computations could be
done in an initialization function, and saved somewhere in the match state.
Then we could pass a pointer to the saved state instead of the match state,
and avoid duplicate computations.

TODO: Move the match re-winding into searchMax. This improves compression
ratio, and unlocks further simplifications with the next TODO.

TODO: Try moving the repcode search into searchMax. After the re-winding
and repcode search are in searchMax, there is no more logic in the match
finder loop that requires knowledge about the dictMode. So we should be
able to avoid force inlining it, and we can join the extDict loop with
the single segment loop. It should go in searchMax instead of its own
function to avoid having multiple virtual function calls per search.


---

## `static ZSTD_LazyVTable const *ZSTD_selectLazyVTable(ZSTD_matchState_t const *ms, searchMethod_e searchMethod, ZSTD_dictMode_e dictMode) {`


This table is indexed first by the four ZSTD_dictMode_e values, and then
by the two searchMethod_e values. NULLs are placed for configurations
that should never occur (extDict modes go to the other implementation
below and there is no DDSS for binary tree search yet).


---

