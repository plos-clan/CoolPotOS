# lib/tinycrypt/ctr_prng.c

## `static void arrInc(uint8_t arr[], unsigned int len) {`


Array incrementer
Treats the supplied array as one contiguous number (MSB in arr[0]), and
increments it by one

- **Returns**: none
- **`arr`**: IN/OUT -- array to be incremented
- **`len`**: IN -- size of arr in bytes


---

## `static void tc_ctr_prng_update(TCCtrPrng_t *const ctx, uint8_t const *const providedData) {`


CTR PRNG update
Updates the internal state of supplied the CTR PRNG context
increments it by one

- **Returns**: none

> **Note:** Assumes: providedData is (TC_AES_KEY_SIZE + TC_AES_BLOCK_SIZE) bytes long

- **`ctx`**: IN/OUT -- CTR PRNG state
- **`providedData`**: IN -- data used when updating the internal state


---

