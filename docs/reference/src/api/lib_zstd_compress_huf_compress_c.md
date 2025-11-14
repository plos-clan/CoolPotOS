# lib/zstd/compress/huf_compress.c

## `static U32 HUF_setMaxHeight(nodeElt *huffNode, U32 lastNonNull, U32 maxNbBits) {`


HUF_setMaxHeight():
Enforces maxNbBits on the Huffman tree described in huffNode.

It sets all nodes with nbBits > maxNbBits to be maxNbBits. Then it adjusts
the tree to so that it is a valid canonical Huffman tree.

@pre               The sum of the ranks of each symbol == 2^largestBits,
where largestBits == huffNode[lastNonNull].nbBits.
@post              The sum of the ranks of each symbol == 2^largestBits,
where largestBits is the return value <= maxNbBits.


- **`huffNode`**: The Huffman tree modified in place to enforce maxNbBits.
- **`lastNonNull`**: The symbol with the lowest count in the Huffman tree.
- **`maxNbBits`**: The maximum allowed number of bits, which the Huffman tree
may not respect. After this function the Huffman tree will
respect maxNbBits.

- **Returns**: The maximum number of bits of the Huffman tree after adjustment,
necessarily no more than maxNbBits.


---

## `static void HUF_sort(nodeElt huffNode[], const unsigned count[], U32 const maxSymbolValue, rankPos rankPosition[]) {`


HUF_sort():
Sorts the symbols [0, maxSymbolValue] by count[symbol] in decreasing order.
This is a typical bucket sorting strategy that uses either quicksort or insertion sort to sort each bucket.


- **`[out]`**: huffNode       Sorted symbols by decreasing count. Only members `.count` and `.byte` are filled.
Must have (maxSymbolValue + 1) entries.

- **`[in]`**: count          Histogram of the symbols.
- **`[in]`**: maxSymbolValue Maximum symbol value.
- **`rankPosition`**: This is a scratch workspace. Must have RANK_POSITION_TABLE_SIZE entries.


---

## `#define STARTNODE (HUF_SYMBOLVALUE_MAX + 1) /* HUF_buildTree(): * Takes the huffNode array sorted by HUF_sort() and builds an unlimited-depth Huffman tree. * * @param huffNode The array sorted by HUF_sort(). Builds the Huffman tree in this array. * @param maxSymbolValue The maximum symbol value. * @return The smallest node in the Huffman tree (by count). */ static int HUF_buildTree(nodeElt *huffNode, U32 maxSymbolValue) {`

HUF_buildCTable_wksp() :
Same as HUF_buildCTable(), but using externally allocated scratch buffer.
`workSpace` must be aligned on 4-bytes boundaries, and be at least as large as sizeof(HUF_buildCTable_wksp_tables).


---

## `static void HUF_buildCTableFromTree(HUF_CElt *CTable, nodeElt const *huffNode, int nonNullRank, U32 maxSymbolValue, U32 maxNbBits) {`


HUF_buildCTableFromTree():
Build the CTable given the Huffman tree in huffNode.


- **`[out]`**: CTable         The output Huffman CTable.
- **`huffNode`**: The Huffman tree.
- **`nonNullRank`**: The last and smallest node in the Huffman tree.
- **`maxSymbolValue`**: The maximum symbol value.
- **`maxNbBits`**: The exact maximum number of bits used in the Huffman tree.


---

## `#define HUF_BITS_IN_CONTAINER (sizeof(size_t) * 8) typedef struct {`

HUF_CStream_t:
Huffman uses its own BIT_CStream_t implementation.
There are three major differences from BIT_CStream_t:
1. HUF_addBits() takes a HUF_CElt (size_t) which is
the pair (nbBits, value) in the format:
format:
- Bits [0, 4)            = nbBits
- Bits [4, 64 - nbBits)  = 0
- Bits [64 - nbBits, 64) = value
2. The bitContainer is built from the upper bits and
right shifted. E.g. to add a new value of N bits
you right shift the bitContainer by N, then or in
the new value into the N upper bits.
3. The bitstream has two bit containers. You can add
bits to the second container and merge them into
the first container.


---

## `static size_t HUF_initCStream(HUF_CStream_t *bitC, void *startPtr, size_t dstCapacity) {`

! HUF_initCStream():
Initializes the bitstream.

- **Returns**: s 0 or an error code.


---

## `static size_t HUF_tightCompressBound(size_t srcSize, size_t tableLog) {`


Returns a tight upper bound on the output space needed by Huffman
with 8 bytes buffer to handle over-writes. If the output is at least
this large we don't need to do bounds checks during Huffman encoding.


---

## `size_t HUF_buildCTable(HUF_CElt *tree, const unsigned *count, unsigned maxSymbolValue, unsigned maxNbBits) {`

HUF_buildCTable() :

- **Returns**: : maxNbBits
Note : count is used before tree is written, so they can safely overlap


---

