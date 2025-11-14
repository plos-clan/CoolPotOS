# lib/zstd/compress/hist.h

## `size_t HIST_count_wksp(unsigned* count, unsigned* maxSymbolValuePtr, const void* src, size_t srcSize, void* workSpace, size_t workSpaceSize);`

HIST_count_wksp() :
Same as HIST_count(), but using an externally provided scratch buffer.
Benefit is this function will use very little stack space.
`workSpace` is a writable buffer which must be 4-bytes aligned,
`workSpaceSize` must be >= HIST_WKSP_SIZE


---

## `size_t HIST_countFast(unsigned* count, unsigned* maxSymbolValuePtr, const void* src, size_t srcSize);`

HIST_countFast() :
same as HIST_count(), but blindly trusts that all byte values within src are <= *maxSymbolValuePtr.
This function is unsafe, and will segfault if any value within `src` is `> *maxSymbolValuePtr`


---

## `size_t HIST_countFast_wksp(unsigned* count, unsigned* maxSymbolValuePtr, const void* src, size_t srcSize, void* workSpace, size_t workSpaceSize);`

HIST_countFast_wksp() :
Same as HIST_countFast(), but using an externally provided scratch buffer.
`workSpace` is a writable buffer which must be 4-bytes aligned,
`workSpaceSize` must be >= HIST_WKSP_SIZE


---

