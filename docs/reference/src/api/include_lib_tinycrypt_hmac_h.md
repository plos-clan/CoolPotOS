# include/lib/tinycrypt/hmac.h

## `#ifndef __TC_HMAC_H__ #define __TC_HMAC_H__ #include <lib/tinycrypt/sha256.h> #ifdef __cplusplus extern "C" {`


@file
Interface to an HMAC implementation.

Overview:   HMAC is a message authentication code based on hash functions.
TinyCrypt hard codes SHA-256 as the hash function. A message
authentication code based on hash functions is also called a
keyed cryptographic hash function since it performs a
transformation specified by a key in an arbitrary length data
set into a fixed length data set (also called tag).

Security:   The security of the HMAC depends on the length of the key and
on the security of the hash function. Note that HMAC primitives
are much less affected by collision attacks than their
corresponding hash functions.

Requires:   SHA-256

Usage:      1) call tc_hmac_set_key to set the HMAC key.

2) call tc_hmac_init to initialize a struct hash_state before
processing the data.

3) call tc_hmac_update to process the next input segment;
tc_hmac_update can be called as many times as needed to process
all of the segments of the input; the order is important.

4) call tc_hmac_final to out put the tag.


---

## `int tc_hmac_set_key(TCHmacState_t ctx, const uint8_t *key, unsigned int key_size);`


HMAC set key procedure
Configures ctx to use key

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if
ctx == NULL or
key == NULL or
key_size == 0

- **`ctx`**: IN/OUT -- the struct tc_hmac_state_struct to initial
- **`key`**: IN -- the HMAC key to configure
- **`key_size`**: IN -- the HMAC key size


---

## `int tc_hmac_init(TCHmacState_t ctx);`


HMAC init procedure
Initializes ctx to begin the next HMAC operation

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if: ctx == NULL or key == NULL

- **`ctx`**: IN/OUT -- struct tc_hmac_state_struct buffer to init


---

## `int tc_hmac_update(TCHmacState_t ctx, const void *data, unsigned int data_length);`


HMAC update procedure
Mixes data_length bytes addressed by data into state

- **Returns**: returns TC_CRYPTO_SUCCCESS (1)
returns TC_CRYPTO_FAIL (0) if: ctx == NULL or key == NULL

> **Note:** Assumes state has been initialized by tc_hmac_init

- **`ctx`**: IN/OUT -- state of HMAC computation so far
- **`data`**: IN -- data to incorporate into state
- **`data_length`**: IN -- size of data in bytes


---

## `int tc_hmac_final(uint8_t *tag, unsigned int taglen, TCHmacState_t ctx);`


HMAC final procedure
Writes the HMAC tag into the tag buffer

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if:
tag == NULL or
ctx == NULL or
key == NULL or
taglen != TC_SHA256_DIGEST_SIZE

> **Note:** ctx is erased before exiting. This should never be changed/removed.

> **Note:** Assumes the tag bufer is at least sizeof(hmac_tag_size(state)) bytes
state has been initialized by tc_hmac_init

- **`tag`**: IN/OUT -- buffer to receive computed HMAC tag
- **`taglen`**: IN -- size of tag in bytes
- **`ctx`**: IN/OUT -- the HMAC state for computing tag


---

