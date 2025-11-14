# lib/zstd/common/fse.h

## `/*-**************************************** * FSE simple functions ******************************************/ /*! FSE_compress() : Compress content of buffer 'src', of size 'srcSize', into destination buffer 'dst'. 'dst' buffer must be already allocated. Compression runs faster is dstCapacity >= FSE_compressBound(srcSize). @return : size of compressed data (<= dstCapacity). Special values : if return == 0, srcData is not compressible => Nothing is stored within dst !!! if return == 1, srcData is a single byte symbol * srcSize times. Use RLE compression instead. if FSE_isError(return), compression failed (more details using FSE_getErrorName()) */ FSE_PUBLIC_API size_t FSE_compress(void* dst, size_t dstCapacity, const void* src, size_t srcSize);`

< library version number; to be used when checking dll version 

---

## `/* FSE_compress_wksp() : * Same as FSE_compress2(), but using an externally allocated scratch buffer (`workSpace`). * FSE_COMPRESS_WKSP_SIZE_U32() provides the minimum size required for `workSpace` as a table of FSE_CTable. */ #define FSE_COMPRESS_WKSP_SIZE_U32(maxTableLog, maxSymbolValue) ( FSE_CTABLE_SIZE_U32(maxTableLog, maxSymbolValue) + ((maxTableLog > 12) ? (1 << (maxTableLog - 2)) : 1024) ) size_t FSE_compress_wksp (void* dst, size_t dstSize, const void* src, size_t srcSize, unsigned maxSymbolValue, unsigned tableLog, void* workSpace, size_t wkspSize);`

< same as FSE_optimalTableLog(), which used `minus==2` 

---

## `size_t FSE_buildCTable_rle (FSE_CTable* ct, unsigned char symbolValue);`

< build a fake FSE_CTable, designed for a flat distribution, where each symbol uses nbBits 

---

## `/* FSE_buildCTable_wksp() : * Same as FSE_buildCTable(), but using an externally allocated scratch buffer (`workSpace`). * `wkspSize` must be >= `FSE_BUILD_CTABLE_WORKSPACE_SIZE_U32(maxSymbolValue, tableLog)` of `unsigned`. * See FSE_buildCTable_wksp() for breakdown of workspace usage. */ #define FSE_BUILD_CTABLE_WORKSPACE_SIZE_U32(maxSymbolValue, tableLog) (((maxSymbolValue + 2) + (1ull << (tableLog)))/2 + sizeof(U64)/sizeof(U32) /* additional 8 bytes for potential table overwrite */) #define FSE_BUILD_CTABLE_WORKSPACE_SIZE(maxSymbolValue, tableLog) (sizeof(unsigned) * FSE_BUILD_CTABLE_WORKSPACE_SIZE_U32(maxSymbolValue, tableLog)) size_t FSE_buildCTable_wksp(FSE_CTable* ct, const short* normalizedCounter, unsigned maxSymbolValue, unsigned tableLog, void* workSpace, size_t wkspSize);`

< build a fake FSE_CTable, designed to compress always the same symbolValue 

---

## `size_t FSE_buildDTable_raw (FSE_DTable* dt, unsigned nbBits);`

< Same as FSE_buildDTable(), using an externally allocated `workspace` produced with `FSE_BUILD_DTABLE_WKSP_SIZE_U32(maxSymbolValue)` 

---

## `size_t FSE_buildDTable_rle (FSE_DTable* dt, unsigned char symbolValue);`

< build a fake FSE_DTable, designed to read a flat distribution where each symbol uses nbBits 

---

## `#define FSE_DECOMPRESS_WKSP_SIZE_U32(maxTableLog, maxSymbolValue) (FSE_DTABLE_SIZE_U32(maxTableLog) + FSE_BUILD_DTABLE_WKSP_SIZE_U32(maxTableLog, maxSymbolValue) + (FSE_MAX_SYMBOL_VALUE + 1) / 2 + 1) #define FSE_DECOMPRESS_WKSP_SIZE(maxTableLog, maxSymbolValue) (FSE_DECOMPRESS_WKSP_SIZE_U32(maxTableLog, maxSymbolValue) * sizeof(unsigned)) size_t FSE_decompress_wksp(void* dst, size_t dstCapacity, const void* cSrc, size_t cSrcSize, unsigned maxLog, void* workSpace, size_t wkspSize);`

< build a fake FSE_DTable, designed to always generate the same symbolValue 

---

## `size_t FSE_decompress_wksp_bmi2(void* dst, size_t dstCapacity, const void* cSrc, size_t cSrcSize, unsigned maxLog, void* workSpace, size_t wkspSize, int bmi2);`

< same as FSE_decompress(), using an externally allocated `workSpace` produced with `FSE_DECOMPRESS_WKSP_SIZE_U32(maxLog, maxSymbolValue)` 

---

## `typedef enum {`

< Same as FSE_decompress_wksp() but with dynamic BMI2 support. Pass 1 if your CPU supports BMI2 or 0 if it doesn't. 

---

## `} FSE_repeat;`

< Can use the previous table and it is assumed to be valid 

---

## `/* ***************************************** * FSE symbol decompression API *******************************************/ typedef struct {`

<
These functions are inner components of FSE_compress_usingCTable().
They allow the creation of custom streams, mixing multiple tables and bit sources.

A key property to keep in mind is that encoding and decoding are done **in reverse direction**.
So the first symbol you will encode is the last you will decode, like a LIFO stack.

You will need a few variables to track your CStream. They are :

FSE_CTable    ct;         // Provided by FSE_buildCTable()
BIT_CStream_t bitStream;  // bitStream tracking structure
FSE_CState_t  state;      // State tracking structure (can have several)


The first thing to do is to init bitStream and state.
size_t errorCode = BIT_initCStream(&bitStream, dstBuffer, maxDstSize);
FSE_initCState(&state, ct);

Note that BIT_initCStream() can produce an error code, so its result should be tested, using FSE_isError();
You can then encode your input data, byte after byte.
FSE_encodeSymbol() outputs a maximum of 'tableLog' bits at a time.
Remember decoding will be done in reverse direction.
FSE_encodeByte(&bitStream, &state, symbol);

At any time, you can also add any bit sequence.
Note : maximum allowed nbBits is 25, for compatibility with 32-bits decoders
BIT_addBits(&bitStream, bitField, nbBits);

The above methods don't commit data to memory, they just store it into local register, for speed.
Local register size is 64-bits on 64-bits systems, 32-bits on 32-bits systems (size_t).
Writing data to memory is a manual operation, performed by the flushBits function.
BIT_flushBits(&bitStream);

Your last FSE encoding operation shall be to flush your last state value(s).
FSE_flushState(&bitStream, &state);

Finally, you must close the bitStream.
The function returns the size of CStream in bytes.
If data couldn't fit into dstBuffer, it will return a 0 ( == not compressible)
If there is an error, it returns an errorCode (which can be tested using FSE_isError()).
size_t size = BIT_closeCStream(&bitStream);

---

## `/* ***************************************** * FSE unsafe API *******************************************/ static unsigned char FSE_decodeSymbolFast(FSE_DState_t* DStatePtr, BIT_DStream_t* bitD);`

<
Let's now decompose FSE_decompress_usingDTable() into its unitary components.
You will decode FSE-encoded symbols from the bitStream,
and also any other bitFields you put in, **in reverse order**.

You will need a few variables to track your bitStream. They are :

BIT_DStream_t DStream;    // Stream context
FSE_DState_t  DState;     // State context. Multiple ones are possible
FSE_DTable*   DTablePtr;  // Decoding table, provided by FSE_buildDTable()

The first thing to do is to init the bitStream.
errorCode = BIT_initDStream(&DStream, srcBuffer, srcSize);

You should then retrieve your initial state(s)
(in reverse flushing order if you have several ones) :
errorCode = FSE_initDState(&DState, &DStream, DTablePtr);

You can then decode your data, symbol after symbol.
For information the maximum number of bits read by FSE_decodeSymbol() is 'tableLog'.
Keep in mind that symbols are decoded in reverse order, like a LIFO stack (last in, first out).
unsigned char symbol = FSE_decodeSymbol(&DState, &DStream);

You can retrieve any bitfield you eventually stored into the bitStream (in reverse order)
Note : maximum allowed nbBits is 25, for 32-bits compatibility
size_t bitField = BIT_readBits(&DStream, nbBits);

All above operations only read from local register (which size depends on size_t).
Refueling the register from memory is manually performed by the reload method.
endSignal = FSE_reloadDStream(&DStream);

BIT_reloadDStream() result tells if there is still some more data to read from DStream.
BIT_DStream_unfinished : there is still some data left into the DStream.
BIT_DStream_endOfBuffer : Dstream reached end of buffer. Its container may no longer be completely filled.
BIT_DStream_completed : Dstream reached its exact end, corresponding in general to decompression completed.
BIT_DStream_tooFar : Dstream went too far. Decompression result is corrupted.

When reaching end of buffer (BIT_DStream_endOfBuffer), progress slowly, notably if you decode multiple symbols per loop,
to properly detect the exact end of stream.
After each decoded symbol, check if DStream is fully consumed using this simple test :
BIT_reloadDStream(&DStream) >= BIT_DStream_completed

When it's done, verify decompression is fully completed, by checking both DStream and the relevant states.
Checking if DStream has reached its end is performed by :
BIT_endOfDStream(&DStream);
Check also the states. There might be some symbols left there, if some high probability ones (>50%) are possible.
FSE_endOfDState(&DState);

---

