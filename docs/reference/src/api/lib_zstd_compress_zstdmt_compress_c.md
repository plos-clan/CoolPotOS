# lib/zstd/compress/zstdmt_compress.c

## `static buffer_t ZSTDMT_getBuffer(ZSTDMT_bufferPool *bufPool) {`

ZSTDMT_getBuffer() :
assumption : bufPool must be valid

- **Returns**: : a buffer, with start pointer and size
note: allocation may fail, in this case, start==NULL and size==0 

---

## `static buffer_t ZSTDMT_resizeBuffer(ZSTDMT_bufferPool *bufPool, buffer_t buffer) {`

ZSTDMT_resizeBuffer() :
assumption : bufPool must be valid

- **Returns**: : a buffer that is at least the buffer pool buffer size.
If a reallocation happens, the data in the input buffer is copied.


---

## `static range_t ZSTDMT_getInputDataInUse(ZSTDMT_CCtx *mtctx) {`


Returns the range of data used by the earliest job that is not yet complete.
If the data of the first job is broken up into two segments, we cover both
sections.


---

## `static int ZSTDMT_isOverlapped(buffer_t buffer, range_t range) {`


Returns non-zero iff buffer and range overlap.


---

## `static int ZSTDMT_tryGetInputRange(ZSTDMT_CCtx *mtctx) {`


Attempts to set the inBuff to the next section to fill.
If any part of the new section is still in use we give up.
Returns non-zero if the buffer is filled.


---

## `static syncPoint_t findSynchronizationPoint(ZSTDMT_CCtx const *mtctx, ZSTD_inBuffer const input) {`


Searches through the input for a synchronization point. If one is found, we
will instruct the caller to flush, and return the number of bytes to load.
Otherwise, we will load as many bytes as possible and instruct the caller
to continue as normal.


---

## `size_t ZSTDMT_compressStream_generic(ZSTDMT_CCtx *mtctx, ZSTD_outBuffer *output, ZSTD_inBuffer *input, ZSTD_EndDirective endOp) {`

ZSTDMT_compressStream_generic() :
internal use only - exposed to be invoked from zstd_compress.c
assumption : output and input are valid (pos <= size)

- **Returns**: : minimum amount of data remaining to flush, 0 if none 

---

