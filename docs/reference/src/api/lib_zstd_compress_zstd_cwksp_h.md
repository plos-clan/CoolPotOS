# lib/zstd/compress/zstd_cwksp.h

## `typedef enum {`


Used to describe whether the workspace is statically allocated (and will not
necessarily ever be freed), or if it's dynamically allocated and we can
expect a well-formed caller to free this.


---

## `typedef struct {`


Zstd fits all its internal datastructures into a single continuous buffer,
so that it only needs to perform a single OS allocation (or so that a buffer
can be provided to it and it can perform no allocations at all). This buffer
is called the workspace.

Several optimizations complicate that process of allocating memory ranges
from this workspace for each internal datastructure:

- These different internal datastructures have different setup requirements:

- The static objects need to be cleared once and can then be trivially
reused for each compression.

- Various buffers don't need to be initialized at all--they are always
written into before they're read.

- The matchstate tables have a unique requirement that they don't need
their memory to be totally cleared, but they do need the memory to have
some bound, i.e., a guarantee that all values in the memory they've been
allocated is less than some maximum value (which is the starting value
for the indices that they will then use for compression). When this
guarantee is provided to them, they can use the memory without any setup
work. When it can't, they have to clear the area.

- These buffers also have different alignment requirements.

- We would like to reuse the objects in the workspace for multiple
compressions without having to perform any expensive reallocation or
reinitialization work.

- We would like to be able to efficiently reuse the workspace across
multiple compressions **even when the compression parameters change** and
we need to resize some of the objects (where possible).

To attempt to manage this buffer, given these constraints, the ZSTD_cwksp
abstraction was created. It works as follows:

Workspace Layout:

[                        ... workspace ...                         ]
[objects][tables ... ->] free space [<- ... aligned][<- ... buffers]

The various objects that live in the workspace are divided into the
following categories, and are allocated separately:

- Static objects: this is optionally the enclosing ZSTD_CCtx or ZSTD_CDict,
so that literally everything fits in a single buffer. Note: if present,
this must be the first object in the workspace, since ZSTD_customFree{CCtx,
CDict}() rely on a pointer comparison to see whether one or two frees are
required.

- Fixed size objects: these are fixed-size, fixed-count objects that are
nonetheless "dynamically" allocated in the workspace so that we can
control how they're initialized separately from the broader ZSTD_CCtx.
Examples:
- Entropy Workspace
- 2 x ZSTD_compressedBlockState_t
- CDict dictionary contents

- Tables: these are any of several different datastructures (hash tables,
chain tables, binary trees) that all respect a common format: they are
uint32_t arrays, all of whose values are between 0 and (nextSrc - base).
Their sizes depend on the cparams. These tables are 64-byte aligned.

- Aligned: these buffers are used for various purposes that require 4 byte
alignment, but don't require any initialization before they're used. These
buffers are each aligned to 64 bytes.

- Buffers: these buffers are used for various purposes that don't require
any alignment or initialization before they're used. This means they can
be moved around at no cost for a new compression.

Allocating Memory:

The various types of objects must be allocated in order, so they can be
correctly packed into the workspace buffer. That order is:

1. Objects
2. Buffers
3. Aligned/Tables

Attempts to reserve objects of different types out of order will fail.


---

## `MEM_STATIC size_t ZSTD_cwksp_align(size_t size, size_t const align) {`


Align must be a power of 2.


---

## `MEM_STATIC size_t ZSTD_cwksp_alloc_size(size_t size) {`


Use this to determine how much space in the workspace we will consume to
allocate this object. (Normally it should be exactly the size of the object,
but under special conditions, like ASAN, where we pad each object, it might
be larger.)

Since tables aren't currently redzoned, you don't need to call through this
to figure out how much space you need for the matchState tables. Everything
else is though.

Do not use for sizing aligned buffers. Instead, use ZSTD_cwksp_aligned_alloc_size().


---

## `MEM_STATIC size_t ZSTD_cwksp_aligned_alloc_size(size_t size) {`


Returns an adjusted alloc size that is the nearest larger multiple of 64 bytes.
Used to determine the number of bytes required for a given "aligned".


---

## `MEM_STATIC size_t ZSTD_cwksp_slack_space_required(void) {`


Returns the amount of additional space the cwksp must allocate
for internal purposes (currently only alignment).


---

## `MEM_STATIC size_t ZSTD_cwksp_bytes_to_align_ptr(void* ptr, const size_t alignBytes) {`


Return the number of additional bytes required to align a pointer to the given number of bytes.
alignBytes must be a power of two.


---

## `MEM_STATIC void* ZSTD_cwksp_reserve_internal_buffer_space(ZSTD_cwksp* ws, size_t const bytes) {`


Internal function. Do not use directly.
Reserves the given number of bytes within the aligned/buffer segment of the wksp,
which counts from the end of the wksp (as opposed to the object/table segment).

Returns a pointer to the beginning of that space.


---

## `MEM_STATIC size_t ZSTD_cwksp_internal_advance_phase(ZSTD_cwksp* ws, ZSTD_cwksp_alloc_phase_e phase) {`


Moves the cwksp to the next phase, and does any necessary allocations.
cwksp initialization must necessarily go through each phase in order.
Returns a 0 on success, or zstd error


---

## `MEM_STATIC int ZSTD_cwksp_owns_buffer(const ZSTD_cwksp* ws, const void* ptr) {`


Returns whether this object/buffer/etc was allocated in this workspace.


---

## `MEM_STATIC void* ZSTD_cwksp_reserve_internal(ZSTD_cwksp* ws, size_t bytes, ZSTD_cwksp_alloc_phase_e phase) {`


Internal function. Do not use directly.


---

## `MEM_STATIC BYTE* ZSTD_cwksp_reserve_buffer(ZSTD_cwksp* ws, size_t bytes) {`


Reserves and returns unaligned memory.


---

## `MEM_STATIC void* ZSTD_cwksp_reserve_aligned(ZSTD_cwksp* ws, size_t bytes) {`


Reserves and returns memory sized on and aligned on ZSTD_CWKSP_ALIGNMENT_BYTES (64 bytes).


---

## `MEM_STATIC void* ZSTD_cwksp_reserve_table(ZSTD_cwksp* ws, size_t bytes) {`


Aligned on 64 bytes. These buffers have the special property that
their values remain constrained, allowing us to re-use them without
memset()-ing them.


---

## `MEM_STATIC void* ZSTD_cwksp_reserve_object(ZSTD_cwksp* ws, size_t bytes) {`


Aligned on sizeof(void*).
Note : should happen only once, at workspace first initialization


---

## `MEM_STATIC void ZSTD_cwksp_clean_tables(ZSTD_cwksp* ws) {`


Zero the part of the allocated tables not already marked clean.


---

## `MEM_STATIC void ZSTD_cwksp_clear_tables(ZSTD_cwksp* ws) {`


Invalidates table allocations.
All other allocations remain valid.


---

## `MEM_STATIC void ZSTD_cwksp_clear(ZSTD_cwksp* ws) {`


Invalidates all buffer, aligned, and table allocations.
Object allocations remain valid.


---

## `MEM_STATIC void ZSTD_cwksp_init(ZSTD_cwksp* ws, void* start, size_t size, ZSTD_cwksp_static_alloc_e isStatic) {`


The provided workspace takes ownership of the buffer [start, start+size).
Any existing values in the workspace are ignored (the previously managed
buffer, if present, must be separately freed).


---

## `MEM_STATIC void ZSTD_cwksp_move(ZSTD_cwksp* dst, ZSTD_cwksp* src) {`


Moves the management of a workspace from one cwksp to another. The src cwksp
is left in an invalid state (src must be re-init()'ed before it's used again).


---

