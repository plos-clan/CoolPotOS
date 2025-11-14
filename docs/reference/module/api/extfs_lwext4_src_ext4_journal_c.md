# extfs/lwext4/src/ext4_journal.c

## `struct revoke_entry {`

Revoke entry during journal replay.

---

## `ext4_fsblk_t block;`

Block number not to be replayed.

---

## `uint32_t trans_id;`

For any transaction id smaller
than trans_id, records of @block
in those transactions should not
be replayed.

---

## `RB_ENTRY(revoke_entry) revoke_node;`

Revoke tree node.

---

## `struct recover_info {`

Valid journal replay information.

---

## `uint32_t start_trans_id;`

Starting transaction id.

---

## `uint32_t last_trans_id;`

Ending transaction id.

---

## `uint32_t this_trans_id;`

Used as internal argument.

---

## `uint32_t trans_cnt;`

No of transactions went through.

---

## `RB_HEAD(jbd_revoke, revoke_entry) revoke_root;`

RB-Tree storing revoke entries.

---

## `struct replay_arg {`

Journal replay internal arguments.

---

## `struct recover_info *info;`

Journal replay information.

---

## `uint32_t *this_block;`

Current block we are on.

---

## `uint32_t this_trans_id;`

Current trans_id we are on.

---

## `static int jbd_sb_write(struct jbd_fs *jbd_fs, struct jbd_sb *s) {`

Write jbd superblock to disk.

- **`jbd_fs`**: jbd filesystem
- **`s`**: jbd superblock
- **Returns**: standard error code

---

## `static int jbd_sb_read(struct jbd_fs *jbd_fs, struct jbd_sb *s) {`

Read jbd superblock from disk.

- **`jbd_fs`**: jbd filesystem
- **`s`**: jbd superblock
- **Returns**: standard error code

---

## `static bool jbd_verify_sb(struct jbd_sb *sb) {`

Verify jbd superblock.

- **`sb`**: jbd superblock
- **Returns**: true if jbd superblock is valid 

---

## `static int jbd_write_sb(struct jbd_fs *jbd_fs) {`

Write back dirty jbd superblock to disk.

- **`jbd_fs`**: jbd filesystem
- **Returns**: standard error code

---

## `int jbd_get_fs(struct ext4_fs *fs, struct jbd_fs *jbd_fs) {`

Get reference to jbd filesystem.

- **`fs`**: Filesystem to load journal of
- **`jbd_fs`**: jbd filesystem
- **Returns**: standard error code

---

## `int jbd_put_fs(struct jbd_fs *jbd_fs) {`

Put reference of jbd filesystem.

- **`jbd_fs`**: jbd filesystem
- **Returns**: standard error code

---

## `int jbd_inode_bmap(struct jbd_fs *jbd_fs, ext4_lblk_t iblock, ext4_fsblk_t *fblock) {`

Data block lookup helper.

- **`jbd_fs`**: jbd filesystem
- **`iblock`**: block index
- **`fblock`**: logical block address
- **Returns**: standard error code

---

## `static int jbd_block_get(struct jbd_fs *jbd_fs, struct ext4_block *block, ext4_fsblk_t fblock) {`

jbd block get function (through cache).

- **`jbd_fs`**: jbd filesystem
- **`block`**: block descriptor
- **`fblock`**: jbd logical block address
- **Returns**: standard error code

---

## `static int jbd_block_get_noread(struct jbd_fs *jbd_fs, struct ext4_block *block, ext4_fsblk_t fblock) {`

jbd block get function (through cache, don't read).

- **`jbd_fs`**: jbd filesystem
- **`block`**: block descriptor
- **`fblock`**: jbd logical block address
- **Returns**: standard error code

---

## `static int jbd_block_set(struct jbd_fs *jbd_fs, struct ext4_block *block) {`

jbd block set procedure (through cache).

- **`jbd_fs`**: jbd filesystem
- **`block`**: block descriptor
- **Returns**: standard error code

---

## `static int jbd_tag_bytes(struct jbd_fs *jbd_fs) {`

helper functions to calculate
block tag size, not including UUID part.

- **`jbd_fs`**: jbd filesystem
- **Returns**: tag size in bytes

---

## `struct tag_info {`

Tag information. 

---

## `int tag_bytes;`

Tag size in bytes, including UUID part.

---

## `ext4_fsblk_t block;`

block number stored in this tag.

---

## `bool is_escape;`

Is the first 4 bytes of block equals to
JBD_MAGIC_NUMBER? 

---

## `bool uuid_exist;`

whether UUID part exists or not.

---

## `uint8_t uuid[UUID_SIZE];`

UUID content if UUID part exists.

---

## `bool last_tag;`

Is this the last tag? 

---

## `uint32_t checksum;`

crc32c checksum. 

---

## `static int jbd_extract_block_tag(struct jbd_fs *jbd_fs, void *__tag, int tag_bytes, int32_t remain_buf_size, struct tag_info *tag_info) {`

Extract information from a block tag.

- **`__tag`**: pointer to the block tag
- **`tag_bytes`**: block tag size of this jbd filesystem
- **`remain_buf_size`**: size in buffer containing the block tag
- **`tag_info`**: information of this tag.
- **Returns**: EOK when succeed, otherwise return EINVAL.

---

## `static int jbd_write_block_tag(struct jbd_fs *jbd_fs, void *__tag, int32_t remain_buf_size, struct tag_info *tag_info) {`

Write information to a block tag.

- **`__tag`**: pointer to the block tag
- **`remain_buf_size`**: size in buffer containing the block tag
- **`tag_info`**: information of this tag.
- **Returns**: EOK when succeed, otherwise return EINVAL.

---

## `static void jbd_iterate_block_table(struct jbd_fs *jbd_fs, void *__tag_start, int32_t tag_tbl_size, void (*func)(struct jbd_fs *jbd_fs, struct tag_info *tag_info, void *arg), void *arg) {`

Iterate all block tags in a block.

- **`jbd_fs`**: jbd filesystem
- **`__tag_start`**: pointer to the block
- **`tag_tbl_size`**: size of the block
- **`func`**: callback routine to indicate that
a block tag is found

- **`arg`**: additional argument to be passed to func 

---

## `static void jbd_replay_block_tags(struct jbd_fs *jbd_fs, struct tag_info *tag_info, void *__arg) {`

Replay a block in a transaction.

- **`jbd_fs`**: jbd filesystem
- **`tag_info`**: tag_info of the logged block.

---

## `static void jbd_add_revoke_block_tags(struct recover_info *info, ext4_fsblk_t block) {`

Add block address to revoke tree, along with
its transaction id.

- **`info`**: journal replay info
- **`block`**: block address to be replayed.

---

## `static void jbd_build_revoke_tree(struct jbd_fs *jbd_fs, struct jbd_bhdr *header, struct recover_info *info) {`

Add entries in a revoke block to revoke tree.

- **`jbd_fs`**: jbd filesystem
- **`header`**: revoke block header
- **`info`**: journal replay info

---

## `static int jbd_iterate_log(struct jbd_fs *jbd_fs, struct recover_info *info, int action) {`

The core routine of journal replay.

- **`jbd_fs`**: jbd filesystem
- **`info`**: journal replay info
- **`action`**: action needed to be taken
- **Returns**: standard error code

---

## `int jbd_recover(struct jbd_fs *jbd_fs) {`

Replay journal.

- **`jbd_fs`**: jbd filesystem
- **Returns**: standard error code

---

## `int jbd_journal_start(struct jbd_fs *jbd_fs, struct jbd_journal *journal) {`

Start accessing the journal.

- **`jbd_fs`**: jbd filesystem
- **`journal`**: current journal session
- **Returns**: standard error code

---

## `int jbd_journal_stop(struct jbd_journal *journal) {`

Stop accessing the journal.

- **`journal`**: current journal session
- **Returns**: standard error code

---

## `static uint32_t jbd_journal_alloc_block(struct jbd_journal *journal, struct jbd_trans *trans) {`

Allocate a block in the journal.

- **`journal`**: current journal session
- **`trans`**: transaction
- **Returns**: allocated block address

---

## `int jbd_trans_set_block_dirty(struct jbd_trans *trans, struct ext4_block *block) {`

Add block to a transaction and mark it dirty.

- **`trans`**: transaction
- **`block`**: block descriptor
- **Returns**: standard error code

---

## `int jbd_trans_revoke_block(struct jbd_trans *trans, ext4_fsblk_t lba) {`

Add block to be revoked to a transaction

- **`trans`**: transaction
- **`lba`**: logical block address
- **Returns**: standard error code

---

## `int jbd_trans_try_revoke_block(struct jbd_trans *trans, ext4_fsblk_t lba) {`

Try to add block to be revoked to a transaction.
If @lba still remains in an transaction on checkpoint
queue, add @lba as a revoked block to the transaction.

- **`trans`**: transaction
- **`lba`**: logical block address
- **Returns**: standard error code

---

## `void jbd_journal_free_trans(struct jbd_journal *journal, struct jbd_trans *trans, bool abort) {`

Free a transaction

- **`journal`**: current journal session
- **`trans`**: transaction
- **`abort`**: discard all the modifications on the block?

---

## `static int jbd_trans_write_commit_block(struct jbd_trans *trans) {`

Write commit block for a transaction

- **`trans`**: transaction
- **Returns**: standard error code

---

## `static int jbd_journal_prepare(struct jbd_journal *journal, struct jbd_trans *trans) {`

Write descriptor block for a transaction

- **`journal`**: current journal session
- **`trans`**: transaction
- **Returns**: standard error code

---

## `static int jbd_journal_prepare_revoke(struct jbd_journal *journal, struct jbd_trans *trans) {`

Write revoke block for a transaction

- **`journal`**: current journal session
- **`trans`**: transaction
- **Returns**: standard error code

---

## `void jbd_journal_cp_trans(struct jbd_journal *journal, struct jbd_trans *trans) {`

Put references of block descriptors in a transaction.

- **`journal`**: current journal session
- **`trans`**: transaction

---

## `static void jbd_trans_end_write(struct ext4_bcache *bc __unused, struct ext4_buf *buf, int res, void *arg) {`

Update the start block of the journal when
all the contents in a transaction reach the disk.

---

## `static int __jbd_journal_commit_trans(struct jbd_journal *journal, struct jbd_trans *trans) {`

Commit a transaction to the journal immediately.

- **`journal`**: current journal session
- **`trans`**: transaction
- **Returns**: standard error code

---

## `struct jbd_trans *jbd_journal_new_trans(struct jbd_journal *journal) {`

Allocate a new transaction

- **`journal`**: current journal session
- **Returns**: transaction allocated

---

## `int jbd_journal_commit_trans(struct jbd_journal *journal, struct jbd_trans *trans) {`

Commit a transaction to the journal immediately.

- **`journal`**: current journal session
- **`trans`**: transaction
- **Returns**: standard error code

---

