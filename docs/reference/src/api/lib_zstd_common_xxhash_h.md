# lib/zstd/common/xxhash.h

## `/* * The default return values from XXH functions are unsigned 32 and 64 bit * integers. * This the simplest and fastest format for further post-processing. * * However, this leaves open the question of what is the order on the byte level, * since little and big endian conventions will store the same number differently. * * The canonical representation settles this issue by mandating big-endian * convention, the same convention as human-readable numbers (large digits first). * * When writing hash values to storage, sending them over a network, or printing * them, it's highly recommended to use the canonical representation to ensure * portability across a wider range of systems, present and future. * * The following functions allow transformation of hash values to and from * canonical format. */ /*! * @brief Canonical (big endian) representation of @ref XXH32_hash_t. */ typedef struct {`

****   Canonical representation   ******

---

## `/*! * @brief The opaque state struct for the XXH64 streaming API. * * @see XXH64_state_s for details. */ typedef struct XXH64_state_s XXH64_state_t;`

****   Streaming   ******

---

## `typedef struct {`

****   Canonical representation   ******

---

## `/* * Streaming requires state maintenance. * This operation costs memory and CPU. * As a consequence, streaming is slower than one-shot hashing. * For better performance, prefer one-shot functions whenever applicable. */ /*! * @brief The state struct for the XXH3 streaming API. * * @see XXH3_state_s for details. */ typedef struct XXH3_state_s XXH3_state_t;`

****   Streaming   ******

---

## `/* * Streaming requires state maintenance. * This operation costs memory and CPU. * As a consequence, streaming is slower than one-shot hashing. * For better performance, prefer one-shot functions whenever applicable. * * XXH3_128bits uses the same XXH3_state_t as XXH3_64bits(). * Use already declared XXH3_createState() and XXH3_freeState(). * * All reset and streaming functions have same meaning as their 64-bit counterpart. */ XXH_PUBLIC_API XXH_errorcode XXH3_128bits_reset(XXH3_state_t *statePtr);`

****   Streaming   ******

---

## `typedef struct {`

****   Canonical representation   ******

---

## `/*! * @ingroup xxh32_family */ XXH_PUBLIC_API XXH32_state_t *XXH32_createState(void) {`

****   Hash streaming   ******

---

## `/*! * @ingroup xxh32_family * The default return values from XXH functions are unsigned 32 and 64 bit * integers. * * The canonical representation uses big endian convention, the same convention * as human-readable numbers (large digits first). * * This way, hash values can be written into a file or buffer, remaining * comparable across different systems. * * The following functions allow transformation of hash values to and from their * canonical format. */ XXH_PUBLIC_API void XXH32_canonicalFromHash(XXH32_canonical_t *dst, XXH32_hash_t hash) {`

****   Canonical representation   ******

---

## `typedef XXH64_hash_t xxh_u64;`

****   Memory access   ******

---

## `/*! * @} * @defgroup xxh64_impl XXH64 implementation * @ingroup impl * @{`

****   xxh64   ******

---

## `/*! @ingroup xxh64_family*/ XXH_PUBLIC_API XXH64_state_t *XXH64_createState(void) {`

****   Hash Streaming   ******

---

## `/*! @ingroup xxh64_family */ XXH_PUBLIC_API void XXH64_canonicalFromHash(XXH64_canonical_t *dst, XXH64_hash_t hash) {`

**** Canonical representation   ******

---

