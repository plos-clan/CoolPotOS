# lib/zstd/common/zstd_trace.h

## `unsigned version;`


ZSTD_VERSION_NUMBER

This is guaranteed to be the first member of ZSTD_trace.
Otherwise, this struct is not stable between versions. If
the version number does not match your expectation, you
should not interpret the rest of the struct.


---

## `unsigned streaming;`


Non-zero if streaming (de)compression is used.


---

## `unsigned dictionaryID;`


The dictionary ID.


---

## `unsigned dictionaryIsCold;`


Is the dictionary cold?
Only set on decompression.


---

## `size_t dictionarySize;`


The dictionary size or zero if no dictionary.


---

## `size_t uncompressedSize;`


The uncompressed size of the data.


---

## `size_t compressedSize;`


The compressed size of the data.


---

## `struct ZSTD_CCtx_params_s const *params;`


The fully resolved CCtx parameters (NULL on decompression).


---

## `struct ZSTD_CCtx_s const *cctx;`


The ZSTD_CCtx pointer (NULL on decompression).


---

## `struct ZSTD_DCtx_s const *dctx;`


The ZSTD_DCtx pointer (NULL on compression).


---

## `typedef unsigned long long ZSTD_TraceCtx;`


A tracing context. It must be 0 when tracing is disabled.
Otherwise, any non-zero value returned by a tracing begin()
function is presented to any subsequent calls to end().

Any non-zero value is treated as tracing is enabled and not
interpreted by the library.

Two possible uses are:
* A timestamp for when the begin() function was called.
* A unique key identifying the (de)compression, like the
address of the [dc]ctx pointer if you need to track
more information than just a timestamp.


---

## `ZSTD_WEAK_ATTR ZSTD_TraceCtx ZSTD_trace_compress_begin(struct ZSTD_CCtx_s const *cctx);`


Trace the beginning of a compression call.

- **`cctx`**: The dctx pointer for the compression.
It can be used as a key to map begin() to end().

- **Returns**: s Non-zero if tracing is enabled. The return value is
passed to ZSTD_trace_compress_end().


---

## `ZSTD_WEAK_ATTR void ZSTD_trace_compress_end(ZSTD_TraceCtx ctx, ZSTD_Trace const *trace);`


Trace the end of a compression call.

- **`ctx`**: The return value of ZSTD_trace_compress_begin().
- **`trace`**: The zstd tracing info.


---

## `ZSTD_WEAK_ATTR ZSTD_TraceCtx ZSTD_trace_decompress_begin(struct ZSTD_DCtx_s const *dctx);`


Trace the beginning of a decompression call.

- **`dctx`**: The dctx pointer for the decompression.
It can be used as a key to map begin() to end().

- **Returns**: s Non-zero if tracing is enabled. The return value is
passed to ZSTD_trace_compress_end().


---

## `ZSTD_WEAK_ATTR void ZSTD_trace_decompress_end(ZSTD_TraceCtx ctx, ZSTD_Trace const *trace);`


Trace the end of a decompression call.

- **`ctx`**: The return value of ZSTD_trace_decompress_begin().
- **`trace`**: The zstd tracing info.


---

