# lib/zstd/common/pool.c

## `static int isQueueFull(POOL_ctx const *ctx) {`


Returns 1 if the queue is full and 0 otherwise.

When queueSize is 1 (pool was created with an intended queueSize of 0),
then a queue is empty if there is a thread free _and_ no job is waiting.


---

