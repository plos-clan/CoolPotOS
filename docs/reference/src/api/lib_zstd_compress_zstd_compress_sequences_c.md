# lib/zstd/compress/zstd_compress_sequences.c

## `static unsigned const kInverseProbabilityLog256[256] = {`


-log2(x / 256) lookup table for x in [0, 256).
If x == 0: Return 0
Else: Return floor(-log2(x / 256) * 256)


---

## `static unsigned ZSTD_useLowProbCount(size_t const nbSeq) {`


Returns true if we should use ncount=-1 else we should
use ncount=1 for low probability symbols instead.


---

## `static size_t ZSTD_NCountCost(unsigned const *count, unsigned const max, size_t const nbSeq, unsigned const FSELog) {`


Returns the cost in bytes of encoding the normalized count header.
Returns an error if any of the helper functions return an error.


---

## `static size_t ZSTD_entropyCost(unsigned const *count, unsigned const max, size_t const total) {`


Returns the cost in bits of encoding the distribution described by count
using the entropy bound.


---

## `size_t ZSTD_fseBitCost(FSE_CTable const *ctable, unsigned const *count, unsigned const max) {`


Returns the cost in bits of encoding the distribution in count using ctable.
Returns an error if ctable cannot represent all the symbols in count.


---

## `size_t ZSTD_crossEntropyCost(short const *norm, unsigned accuracyLog, unsigned const *count, unsigned const max) {`


Returns the cost in bits of encoding the distribution in count using the
table described by norm. The max symbol support by norm is assumed >= max.
norm must be valid for every symbol with non-zero probability in count.


---

