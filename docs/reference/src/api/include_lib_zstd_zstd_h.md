# include/lib/zstd/zstd.h

## `/*------ Version ------*/ # define ZSTD_VERSION_MAJOR 1 # define ZSTD_VERSION_MINOR 5 # define ZSTD_VERSION_RELEASE 2 # define ZSTD_VERSION_NUMBER \ (ZSTD_VERSION_MAJOR * 100 * 100 + ZSTD_VERSION_MINOR * 100 + ZSTD_VERSION_RELEASE) /*! ZSTD_versionNumber() : * Return runtime library version, the value is (MAJOR*100*100 + MINOR*100 + RELEASE). */ ZSTDLIB_API unsigned ZSTD_versionNumber(void);`

****************************************************************************
Introduction

zstd, short for Zstandard, is a fast lossless compression algorithm, targeting
real-time compression scenarios at zlib-level and better compression ratios.
The zstd compression library provides in-memory compression and decompression
functions.

The library supports regular compression levels from 1 up to ZSTD_maxCLevel(),
which is currently 22. Levels >= 20, labeled `--ultra`, should be used with
caution, as they require more memory. The library also offers negative
compression levels, which extend the range of speed vs. ratio preferences.
The lower the level, the faster the speed (at the cost of compression).

Compression can be done in:
- a single step (described as Simple API)
- a single step, reusing a context (described as Explicit context)
- unbounded multiple steps (described as Streaming compression)

The compression ratio achievable on small data can be highly improved using
a dictionary. Dictionary compression can be performed in:
- a single step (described as Simple dictionary API)
- a single step, reusing a dictionary (described as Bulk-processing
dictionary API)

Advanced experimental functions can be accessed using
`#define ZSTD_STATIC_LINKING_ONLY` before including zstd.h.

Advanced experimental APIs should never be used with a dynamically-linked
library. They are not "stable"; their definitions or signatures may change in
the future. Only static linking is allowed.
*****************************************************************************

---

## `/*! ZSTD_compress() : * Compresses `src` content as a single zstd compressed frame into already allocated `dst`. * Hint : compression runs faster if `dstCapacity` >= `ZSTD_compressBound(srcSize)`. * @return : compressed size written into `dst` (<= `dstCapacity), * or an error code if it fails (which can be tested using ZSTD_isError()). */ ZSTDLIB_API size_t ZSTD_compress(void *dst, size_t dstCapacity, const void *src, size_t srcSize, int compressionLevel);`

************************************
Simple API
*************************************

---

## `/*= Compression context * When compressing many times, * it is recommended to allocate a context just once, * and re-use it for each successive compression operation. * This will make workload friendlier for system's memory. * Note : re-using context is just a speed / resource optimization. * It doesn't change the compression ratio, which remains identical. * Note 2 : In multi-threaded environments, * use one different context per thread for parallel execution. */ typedef struct ZSTD_CCtx_s ZSTD_CCtx;`

************************************
Explicit context
*************************************

---

## `/* API design : * Parameters are pushed one by one into an existing context, * using ZSTD_CCtx_set*() functions. * Pushed parameters are sticky : they are valid for next compressed frame, and any subsequent frame. * "sticky" parameters are applicable to `ZSTD_compress2()` and `ZSTD_compressStream*()` ! * __They do not apply to "simple" one-shot variants such as ZSTD_compressCCtx()__ . * * It's possible to reset all parameters to "default" using ZSTD_CCtx_reset(). * * This API supersedes all other "advanced" API entry points in the experimental section. * In the future, we expect to remove from experimental API entry points which are redundant with this API. */ /* Compression strategies, listed from fastest to strongest */ typedef enum {`

******************************************
Advanced compression API (Requires v1.4.0+)
********************************************

---

## `/* The advanced API pushes parameters one by one into an existing DCtx context. * Parameters are sticky, and remain valid for all following frames * using the same DCtx context. * It's possible to reset parameters to default values using ZSTD_DCtx_reset(). * Note : This API is compatible with existing ZSTD_decompressDCtx() and ZSTD_decompressStream(). * Therefore, no new decompression function is necessary. */ typedef enum {`

********************************************
Advanced decompression API (Requires v1.4.0+)
**********************************************

---

## `typedef struct ZSTD_inBuffer_s {`

*************************
Streaming
**************************

---

## `size_t size;`

< start of input buffer 

---

## `size_t pos;`

< size of input buffer 

---

## `} ZSTD_inBuffer;`

< position where reading stopped. Will be updated. Necessarily 0 <= pos <= size 

---

## `size_t size;`

< start of output buffer 

---

## `size_t pos;`

< size of output buffer 

---

## `} ZSTD_outBuffer;`

< position where writing stopped. Will be updated. Necessarily 0 <= pos <= size 

---

## `/* Continue to distinguish them for compatibility with older versions <= v1.2.0 */ /*===== ZSTD_CStream management functions =====*/ ZSTDLIB_API ZSTD_CStream *ZSTD_createCStream(void);`

< CCtx and CStream are now effectively same object (>= v1.3.0) 

---

## `ZSTDLIB_API size_t ZSTD_CStreamOutSize( void);`

< recommended size for input buffer 

---

## `/* ***************************************************************************** * This following is a legacy streaming API, available since v1.0+ . * It can be replaced by ZSTD_CCtx_reset() and ZSTD_compressStream2(). * It is redundant, but remains fully supported. * Streaming in combination with advanced parameters and dictionary compression * can only be used through the new API. ******************************************************************************/ /*! * Equivalent to: * * ZSTD_CCtx_reset(zcs, ZSTD_reset_session_only);`

< recommended size for output buffer. Guarantee to successfully flush at least one complete compressed block. 

---

## `/* For compatibility with versions <= v1.2.0, prefer differentiating them. */ /*===== ZSTD_DStream management functions =====*/ ZSTDLIB_API ZSTD_DStream *ZSTD_createDStream(void);`

< DCtx and DStream are now effectively same object (>= v1.3.0) 

---

## `/*! ZSTD_compress_usingDict() : * Compression at an explicit compression level using a Dictionary. * A dictionary can be any arbitrary data segment (also called a prefix), * or a buffer with specified information (see zdict.h). * Note : This function loads the dictionary, resulting in significant startup delay. * It's intended for a dictionary used only once. * Note 2 : When `dict == NULL || dictSize < 8` no dictionary is used. */ ZSTDLIB_API size_t ZSTD_compress_usingDict(ZSTD_CCtx *ctx, void *dst, size_t dstCapacity, const void *src, size_t srcSize, const void *dict, size_t dictSize, int compressionLevel);`

***********************
Simple dictionary API
*************************

---

## `typedef struct ZSTD_CDict_s ZSTD_CDict;`

********************************
Bulk processing dictionary API
********************************

---

## `/*! ZSTD_getDictID_fromDict() : Requires v1.4.0+ * Provides the dictID stored within dictionary. * if @return == 0, the dictionary is not conformant with Zstandard specification. * It can still be loaded, but as a content-only dictionary. */ ZSTDLIB_API unsigned ZSTD_getDictID_fromDict(const void *dict, size_t dictSize);`

*****************************
Dictionary helper functions
*****************************

---

## `/*! ZSTD_CCtx_loadDictionary() : Requires v1.4.0+ * Create an internal CDict from `dict` buffer. * Decompression will have to use same dictionary. * @result : 0, or an error code (which can be tested with ZSTD_isError()). * Special: Loading a NULL (or 0-size) dictionary invalidates previous dictionary, * meaning "return to no-dictionary mode". * Note 1 : Dictionary is sticky, it will be used for all future compressed frames. * To return to "no-dictionary" situation, load a NULL dictionary (or reset parameters). * Note 2 : Loading a dictionary involves building tables. * It's also a CPU consuming operation, with non-negligible impact on latency. * Tables are dependent on compression parameters, and for this reason, * compression parameters can no longer be changed after loading a dictionary. * Note 3 :`dict` content will be copied internally. * Use experimental ZSTD_CCtx_loadDictionary_byReference() to reference content instead. * In such a case, dictionary buffer must outlive its users. * Note 4 : Use ZSTD_CCtx_loadDictionary_advanced() * to precisely select how dictionary content must be interpreted. */ ZSTDLIB_API size_t ZSTD_CCtx_loadDictionary(ZSTD_CCtx *cctx, const void *dict, size_t dictSize);`

****************************************************************************
Advanced dictionary and prefix API (Requires v1.4.0+)

This API allows dictionaries to be used with ZSTD_compress2(),
ZSTD_compressStream2(), and ZSTD_decompressDCtx(). Dictionaries are sticky, and
only reset with the context is reset with ZSTD_reset_parameters or
ZSTD_reset_session_and_parameters. Prefixes are single-use.
****************************************************************************

---

## `# define ZSTD_FRAMEHEADERSIZE_PREFIX(format) \ ((format) == ZSTD_f_zstd1 \ ? 5 \ : 1) /* minimum input size required to query frame header size */ # define ZSTD_FRAMEHEADERSIZE_MIN(format) ((format) == ZSTD_f_zstd1 ? 6 : 2) # define ZSTD_FRAMEHEADERSIZE_MAX 18 /* can be useful for static allocation */ # define ZSTD_SKIPPABLEHEADERSIZE 8 /* compression parameter bounds */ # define ZSTD_WINDOWLOG_MAX_32 30 # define ZSTD_WINDOWLOG_MAX_64 31 # define ZSTD_WINDOWLOG_MAX \ ((int)(sizeof(size_t) == 4 ? ZSTD_WINDOWLOG_MAX_32 : ZSTD_WINDOWLOG_MAX_64)) # define ZSTD_WINDOWLOG_MIN 10 # define ZSTD_HASHLOG_MAX ((ZSTD_WINDOWLOG_MAX < 30) ? ZSTD_WINDOWLOG_MAX : 30) # define ZSTD_HASHLOG_MIN 6 # define ZSTD_CHAINLOG_MAX_32 29 # define ZSTD_CHAINLOG_MAX_64 30 # define ZSTD_CHAINLOG_MAX \ ((int)(sizeof(size_t) == 4 ? ZSTD_CHAINLOG_MAX_32 : ZSTD_CHAINLOG_MAX_64)) # define ZSTD_CHAINLOG_MIN ZSTD_HASHLOG_MIN # define ZSTD_SEARCHLOG_MAX (ZSTD_WINDOWLOG_MAX - 1) # define ZSTD_SEARCHLOG_MIN 1 # define ZSTD_MINMATCH_MAX 7 /* only for ZSTD_fast, other strategies are limited to 6 */ # define ZSTD_MINMATCH_MIN 3 /* only for ZSTD_btopt+, faster strategies are limited to 4 */ # define ZSTD_TARGETLENGTH_MAX ZSTD_BLOCKSIZE_MAX # define ZSTD_TARGETLENGTH_MIN \ 0 /* note : comparing this constant to an unsigned results in a tautological test */ # define ZSTD_STRATEGY_MIN ZSTD_fast # define ZSTD_STRATEGY_MAX ZSTD_btultra2 # define ZSTD_OVERLAPLOG_MIN 0 # define ZSTD_OVERLAPLOG_MAX 9 # define ZSTD_WINDOWLOG_LIMIT_DEFAULT \ 27 /* by default, the streaming decoder will refuse any frame * requiring larger than (1<<ZSTD_WINDOWLOG_LIMIT_DEFAULT) window size, * to preserve host's memory from unreasonable requirements. * This limit can be overridden using ZSTD_DCtx_setParameter(,ZSTD_d_windowLogMax,). * The limit does not apply for one-pass decoders (such as ZSTD_decompress()), since no additional memory is allocated */ /* LDM parameter bounds */ # define ZSTD_LDM_HASHLOG_MIN ZSTD_HASHLOG_MIN # define ZSTD_LDM_HASHLOG_MAX ZSTD_HASHLOG_MAX # define ZSTD_LDM_MINMATCH_MIN 4 # define ZSTD_LDM_MINMATCH_MAX 4096 # define ZSTD_LDM_BUCKETSIZELOG_MIN 1 # define ZSTD_LDM_BUCKETSIZELOG_MAX 8 # define ZSTD_LDM_HASHRATELOG_MIN 0 # define ZSTD_LDM_HASHRATELOG_MAX (ZSTD_WINDOWLOG_MAX - ZSTD_HASHLOG_MIN) /* Advanced parameter bounds */ # define ZSTD_TARGETCBLOCKSIZE_MIN 64 # define ZSTD_TARGETCBLOCKSIZE_MAX ZSTD_BLOCKSIZE_MAX # define ZSTD_SRCSIZEHINT_MIN 0 # define ZSTD_SRCSIZEHINT_MAX INT_MAX /* --- Advanced types --- */ typedef struct ZSTD_CCtx_params_s ZSTD_CCtx_params;`

*************************************************************************************
experimental API (static linking only)
***************************************************************************************
The following symbols and constants
are not planned to join "stable API" status in the near future.
They can still change in future versions.
Some of them are planned to remain in the static_only section indefinitely.
Some of them might be removed in the future (especially when redundant with existing stable functions)
**************************************************************************************

---

## `unsigned chainLog;`

< largest match distance : larger == more compression, more memory needed during decompression 

---

## `unsigned hashLog;`

< fully searched segment : larger == more compression, slower, more memory (useless for fast) 

---

## `unsigned searchLog;`

< dispatch table : larger == faster, more memory 

---

## `unsigned minMatch;`

< nb of searches : larger == more compression, slower 

---

## `unsigned targetLength;`

< match length searched : larger == faster decompression, sometimes less compression 

---

## `ZSTD_strategy strategy;`

< acceptable match size for optimal parser (only) : larger == more compression, slower 

---

## `} ZSTD_compressionParameters;`

< see ZSTD_strategy definition above 

---

## `int checksumFlag;`

< 1: content size will be in frame header (when known) 

---

## `int noDictIDFlag;`

< 1: generate a 32-bits checksum using XXH64 algorithm at end of frame, for error detection 

---

## `} ZSTD_frameParameters;`

< 1: no dictID will be saved into frame header (dictID is only useful for dictionary compression) 

---

## `} ZSTD_dictLoadMethod_e;`

< Reference dictionary content -- the dictionary buffer must outlive its users. 

---

## `} ZSTD_literalCompressionMode_e;`

< Always emit uncompressed literals. 

---

## `/*! ZSTD_findDecompressedSize() : * `src` should point to the start of a series of ZSTD encoded and/or skippable frames * `srcSize` must be the _exact_ size of this series * (i.e. there should be a frame boundary at `src + srcSize`) * @return : - decompressed size of all data in all successive frames * - if the decompressed size cannot be determined: ZSTD_CONTENTSIZE_UNKNOWN * - if an error occurred: ZSTD_CONTENTSIZE_ERROR * * note 1 : decompressed size is an optional field, that may not be present, especially in streaming mode. * When `return==ZSTD_CONTENTSIZE_UNKNOWN`, data to decompress could be any size. * In which case, it's necessary to use streaming mode to decompress data. * note 2 : decompressed size is always present when compression is done with ZSTD_compress() * note 3 : decompressed size can be very large (64-bits value), * potentially larger than what local system can handle as a single memory segment. * In which case, it's necessary to use streaming mode to decompress data. * note 4 : If source is untrusted, decompressed size could be wrong or intentionally modified. * Always ensure result fits within application's authorized limits. * Each application can set its own limits. * note 5 : ZSTD_findDecompressedSize handles multiple frames, and so it must traverse the input to * read each contained frame header. This is fast as most of the data is skipped, * however it does mean that all frame data must be present and valid. */ ZSTDLIB_STATIC_API unsigned long long ZSTD_findDecompressedSize(const void *src, size_t srcSize);`

************************************
Frame size functions
*************************************

---

## `/*! ZSTD_estimate*() : * These functions make it possible to estimate memory usage * of a future {`

************************************
Memory management
*************************************

---

## `ZSTDLIB_STATIC_API ZSTD_DCtx *ZSTD_initStaticDCtx(void *workspace, size_t workspaceSize);`

< same as ZSTD_initStaticCCtx() 

---

## `ZSTDLIB_STATIC_API const ZSTD_CDict *ZSTD_initStaticCDict(void *workspace, size_t workspaceSize, const void *dict, size_t dictSize, ZSTD_dictLoadMethod_e dictLoadMethod, ZSTD_dictContentType_e dictContentType, ZSTD_compressionParameters cParams);`

< same as ZSTD_initStaticDCtx() 

---

## `ZSTDLIB_STATIC_API ZSTD_CCtx *ZSTD_createCCtx_advanced(ZSTD_customMem customMem);`

< this constant defers to stdlib's functions 

---

## `/*! ZSTD_createCDict_byReference() : * Create a digested dictionary for compression * Dictionary content is just referenced, not duplicated. * As a consequence, `dictBuffer` **must** outlive CDict, * and its content must remain unmodified throughout the lifetime of CDict. * note: equivalent to ZSTD_createCDict_advanced(), with dictLoadMethod==ZSTD_dlm_byRef */ ZSTDLIB_STATIC_API ZSTD_CDict *ZSTD_createCDict_byReference(const void *dictBuffer, size_t dictSize, int compressionLevel);`

************************************
Advanced compression functions
*************************************

---

## `/*! ZSTD_isFrame() : * Tells if the content of `buffer` starts with a valid Frame Identifier. * Note : Frame Identifier is 4 bytes. If `size < 4`, @return will always be 0. * Note 2 : Legacy Frame Identifiers are considered valid only if Legacy Support is enabled. * Note 3 : Skippable Frame Identifiers are considered valid. */ ZSTDLIB_STATIC_API unsigned ZSTD_isFrame(const void *buffer, size_t size);`

************************************
Advanced decompression functions
*************************************

---

## `/*===== Advanced Streaming compression functions =====*/ /*! ZSTD_initCStream_srcSize() : * This function is DEPRECATED, and equivalent to: * ZSTD_CCtx_reset(zcs, ZSTD_reset_session_only);`

*****************************************************************
Advanced streaming functions
Warning : most of these functions are now redundant with the Advanced API.
Once Advanced API reaches "stable" status,
redundant functions will be deprecated, and then at some point removed.
******************************************************************

---

## `/*===== Buffer-less streaming compression functions =====*/ ZSTDLIB_STATIC_API size_t ZSTD_compressBegin(ZSTD_CCtx *cctx, int compressionLevel);`


Buffer-less streaming compression (synchronous mode)

A ZSTD_CCtx object is required to track streaming operations.
Use ZSTD_createCCtx() / ZSTD_freeCCtx() to manage resource.
ZSTD_CCtx object can be re-used multiple times within successive compression operations.

Start by initializing a context.
Use ZSTD_compressBegin(), or ZSTD_compressBegin_usingDict() for dictionary compression.
It's also possible to duplicate a reference context which has already been initialized, using ZSTD_copyCCtx()

Then, consume your input using ZSTD_compressContinue().
There are some important considerations to keep in mind when using this advanced function :
- ZSTD_compressContinue() has no internal buffer. It uses externally provided buffers only.
- Interface is synchronous : input is consumed entirely and produces 1+ compressed blocks.
- Caller must ensure there is enough space in `dst` to store compressed data under worst case scenario.
Worst case evaluation is provided by ZSTD_compressBound().
ZSTD_compressContinue() doesn't guarantee recover after a failed compression.
- ZSTD_compressContinue() presumes prior input ***is still accessible and unmodified*** (up to maximum distance size, see WindowLog).
It remembers all previous contiguous blocks, plus one separated memory segment (which can itself consists of multiple contiguous blocks)
- ZSTD_compressContinue() detects that prior input has been overwritten when `src` buffer overlaps.
In which case, it will "discard" the relevant memory section from its history.

Finish a frame with ZSTD_compressEnd(), which will write the last block(s) and optional checksum.
It's possible to use srcSize==0, in which case, it will write a final empty block to end the frame.
Without last block mark, frames are considered unfinished (hence corrupted) by compliant decoders.

`ZSTD_CCtx` object can be re-used (ZSTD_compressBegin()) to compress again.

---

## `ZSTDLIB_STATIC_API size_t ZSTD_copyCCtx( ZSTD_CCtx *cctx, const ZSTD_CCtx *preparedCCtx, unsigned long long pledgedSrcSize);`

< note: fails if cdict==NULL 

---

## `ZSTDLIB_STATIC_API size_t ZSTD_compressContinue(ZSTD_CCtx *cctx, void *dst, size_t dstCapacity, const void *src, size_t srcSize);`

<  note: if pledgedSrcSize is not known, use ZSTD_CONTENTSIZE_UNKNOWN 

---

## `ZSTD_DEPRECATED("use advanced API to access custom parameters") size_t ZSTD_compressBegin_usingCDict_advanced( ZSTD_CCtx *const cctx, const ZSTD_CDict *const cdict, ZSTD_frameParameters const fParams, unsigned long long const pledgedSrcSize);`

< pledgedSrcSize : If srcSize is not known at init time, use ZSTD_CONTENTSIZE_UNKNOWN 

---

## `/*===== Buffer-less streaming decompression functions =====*/ typedef enum {`


Buffer-less streaming decompression (synchronous mode)

A ZSTD_DCtx object is required to track streaming operations.
Use ZSTD_createDCtx() / ZSTD_freeDCtx() to manage it.
A ZSTD_DCtx object can be re-used multiple times.

First typical operation is to retrieve frame parameters, using ZSTD_getFrameHeader().
Frame header is extracted from the beginning of compressed frame, so providing only the frame's beginning is enough.
Data fragment must be large enough to ensure successful decoding.
`ZSTD_frameHeaderSize_max` bytes is guaranteed to always be large enough.
@result : 0 : successful decoding, the `ZSTD_frameHeader` structure is correctly filled.
>0 : `srcSize` is too small, please provide at least @result bytes on next attempt.
errorCode, which can be tested using ZSTD_isError().

It fills a ZSTD_frameHeader structure with important information to correctly decode the frame,
such as the dictionary ID, content size, or maximum back-reference distance (`windowSize`).
Note that these values could be wrong, either because of data corruption, or because a 3rd party deliberately spoofs false information.
As a consequence, check that values remain within valid application range.
For example, do not allocate memory blindly, check that `windowSize` is within expectation.
Each application can set its own limits, depending on local restrictions.
For extended interoperability, it is recommended to support `windowSize` of at least 8 MB.

ZSTD_decompressContinue() needs previous data blocks during decompression, up to `windowSize` bytes.
ZSTD_decompressContinue() is very sensitive to contiguity,
if 2 blocks don't follow each other, make sure that either the compressor breaks contiguity at the same place,
or that previous contiguous segment is large enough to properly handle maximum back-reference distance.
There are multiple ways to guarantee this condition.

The most memory efficient way is to use a round buffer of sufficient size.
Sufficient size is determined by invoking ZSTD_decodingBufferSize_min(),
which can @return an error code if required value is too large for current system (in 32-bits mode).
In a round buffer methodology, ZSTD_decompressContinue() decompresses each block next to previous one,
up to the moment there is not enough room left in the buffer to guarantee decoding another full block,
which maximum size is provided in `ZSTD_frameHeader` structure, field `blockSizeMax`.
At which point, decoding can resume from the beginning of the buffer.
Note that already decoded data stored in the buffer should be flushed before being overwritten.

There are alternatives possible, for example using two or more buffers of size `windowSize` each, though they consume more memory.

Finally, if you control the compression process, you can also ignore all buffer size rules,
as long as the encoder and decoder progress in "lock-step",
aka use exactly the same buffer sizes, break contiguity at the same place, etc.

Once buffers are setup, start decompression, with ZSTD_decompressBegin().
If decompression requires a dictionary, use ZSTD_decompressBegin_usingDict() or ZSTD_decompressBegin_usingDDict().

Then use ZSTD_nextSrcSizeToDecompress() and ZSTD_decompressContinue() alternatively.
ZSTD_nextSrcSizeToDecompress() tells how many bytes to provide as 'srcSize' to ZSTD_decompressContinue().
ZSTD_decompressContinue() requires this _exact_ amount of bytes, or it will fail.

@result of ZSTD_decompressContinue() is the number of bytes regenerated within 'dst' (necessarily <= dstCapacity).
It can be zero : it just means ZSTD_decompressContinue() has decoded some metadata item.
It can also be an error code, which can be tested with ZSTD_isError().

A frame is fully decoded when ZSTD_nextSrcSizeToDecompress() returns zero.
Context can then be reset to start a new decompression.

Note : it's possible to know if next input to present is a header or a block, using ZSTD_nextInputType().
This information is not required to properly decode a frame.

== Special case : skippable frames ==

Skippable frames allow integration of user-defined data into a flow of concatenated frames.
Skippable frames will be ignored (skipped) by decompressor.
The format of skippable frames is as follows :
a) Skippable frame ID - 4 Bytes, Little endian format, any value from 0x184D2A50 to 0x184D2A5F
b) Frame Size - 4 Bytes, Little endian format, unsigned 32-bits
c) Frame Content - any content (User Data) of length equal to Frame Size
For skippable frames ZSTD_getFrameHeader() returns zfhPtr->frameType==ZSTD_skippableFrame.
For skippable frames ZSTD_decompressContinue() always returns 0 : it only skips the content.

---

## `/*! ZSTD_getFrameHeader_advanced() : * same as ZSTD_getFrameHeader(), * with added capability to select a format (like ZSTD_f_zstd1_magicless) */ ZSTDLIB_STATIC_API size_t ZSTD_getFrameHeader_advanced(ZSTD_frameHeader *zfhPtr, const void *src, size_t srcSize, ZSTD_format_e format);`

< doesn't consume input 

---

## `ZSTDLIB_STATIC_API size_t ZSTD_decompressBegin(ZSTD_DCtx *dctx);`

< when frame content size is not known, pass in frameContentSize == ZSTD_CONTENTSIZE_UNKNOWN 

---

## `/* ============================ */ /*! Block functions produce and decode raw zstd blocks, without frame metadata. Frame metadata cost is typically ~12 bytes, which can be non-negligible for very small blocks (< 100 bytes). But users will have to take in charge needed metadata to regenerate data, such as compressed and content sizes. A few rules to respect : - Compressing and decompressing require a context structure + Use ZSTD_createCCtx() and ZSTD_createDCtx() - It is necessary to init context before starting + compression : any ZSTD_compressBegin*() variant, including with dictionary + decompression : any ZSTD_decompressBegin*() variant, including with dictionary + copyCCtx() and copyDCtx() can be used too - Block size is limited, it must be <= ZSTD_getBlockSize() <= ZSTD_BLOCKSIZE_MAX == 128 KB + If input is larger than a block size, it's necessary to split input data into multiple blocks + For inputs larger than a single block, consider using regular ZSTD_compress() instead. Frame metadata is not that costly, and quickly becomes negligible as source size grows larger than a block. - When a block is considered not compressible enough, ZSTD_compressBlock() result will be 0 (zero) ! ===> In which case, nothing is produced into `dst` ! + User __must__ test for such outcome and deal directly with uncompressed data + A block cannot be declared incompressible if ZSTD_compressBlock() return value was != 0. Doing so would mess up with statistics history, leading to potential data corruption. + ZSTD_decompressBlock() _doesn't accept uncompressed data as input_ !! + In case of multiple successive blocks, should some of them be uncompressed, decoder must be informed of their existence in order to follow proper history. Use ZSTD_insertBlock() for such a case. */ /*===== Raw zstd block functions =====*/ ZSTDLIB_STATIC_API size_t ZSTD_getBlockSize(const ZSTD_CCtx *cctx);`

Block level API       

---

