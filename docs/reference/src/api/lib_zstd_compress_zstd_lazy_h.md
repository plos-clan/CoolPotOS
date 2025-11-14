# lib/zstd/compress/zstd_lazy.h

## `#define ZSTD_LAZY_DDSS_BUCKET_LOG 2 U32 ZSTD_insertAndFindFirstIndex(ZSTD_matchState_t* ms, const BYTE* ip);`


Dedicated Dictionary Search Structure bucket log. In the
ZSTD_dedicatedDictSearch mode, the hashTable has
2 ** ZSTD_LAZY_DDSS_BUCKET_LOG entries in each bucket, rather than just
one.


---

