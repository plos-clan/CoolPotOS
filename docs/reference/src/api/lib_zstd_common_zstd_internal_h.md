# lib/zstd/common/zstd_internal.h

## `MEM_STATIC ZSTD_sequenceLength ZSTD_getSequenceLength(seqStore_t const *seqStore, seqDef const *seq) {`


Returns the ZSTD_sequenceLength for the given sequences. It handles the decoding of long sequences
indicated by longLengthPos and longLengthType, and adds MINMATCH back to matchLength.


---

## `typedef struct {`


Contains the compressed frame size and an upper-bound for the decompressed frame size.
Note: before using `compressedSize`, check for errors using ZSTD_isError().
similarly, before using `decompressedBound`, check for errors using:
`decompressedBound != ZSTD_CONTENTSIZE_ERROR`


---

## `MEM_STATIC unsigned ZSTD_countTrailingZeros(size_t val) {`


Counts the number of trailing zeros of a `size_t`.
Most compilers should support CTZ as a builtin. A backup
implementation is provided if the builtin isn't supported, but
it may not be terribly efficient.


---

## `MEM_STATIC int ZSTD_cpuSupportsBmi2(void) {`



- **Returns**: s true iff the CPU supports dynamic BMI2 dispatch.


---

