# lib/zstd/compress/zstd_fast.c

## `FORCE_INLINE_TEMPLATE size_t ZSTD_compressBlock_fast_noDict_generic( ZSTD_matchState_t *ms, seqStore_t *seqStore, U32 rep[ZSTD_REP_NUM], void const *src, size_t srcSize, U32 const mls, U32 const hasStep) {`


If you squint hard enough (and ignore repcodes), the search operation at any
given position is broken into 4 stages:

1. Hash   (map position to hash value via input read)
2. Lookup (map hash val to index via hashtable read)
3. Load   (map index to value at that position via input read)
4. Compare

Each of these steps involves a memory read at an address which is computed
from the previous step. This means these steps must be sequenced and their
latencies are cumulative.

Rather than do 1->2->3->4 sequentially for a single position before moving
onto the next, this implementation interleaves these operations across the
next few positions:

R = Repcode Read & Compare
H = Hash
T = Table Lookup
M = Match Read & Compare

Pos | Time -->
----+-------------------
N   | ... M
N+1 | ...   TM
N+2 |    R H   T M
N+3 |         H    TM
N+4 |           R H   T M
N+5 |                H   ...
N+6 |                  R ...

This is very much analogous to the pipelining of execution in a CPU. And just
like a CPU, we have to dump the pipeline when we find a match (i.e., take a
branch).

When this happens, we throw away our current state, and do the following prep
to re-enter the loop:

Pos | Time -->
----+-------------------
N   | H T
N+1 |  H

This is also the work we do at the beginning to enter the loop initially.


---

