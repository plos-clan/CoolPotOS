# include/lib/tinycrypt/sha256.h

## `#ifndef __TC_SHA256_H__ #define __TC_SHA256_H__ #include "types.h" #ifdef __cplusplus extern "C" {`


@file
Interface to a SHA-256 implementation.

Overview:   SHA-256 is a NIST approved cryptographic hashing algorithm
specified in FIPS 180. A hash algorithm maps data of arbitrary
size to data of fixed length.

Security:   SHA-256 provides 128 bits of security against collision attacks
and 256 bits of security against pre-image attacks. SHA-256 does
NOT behave like a random oracle, but it can be used as one if
the string being hashed is prefix-free encoded before hashing.

Usage:      1) call tc_sha256_init to initialize a struct
tc_sha256_state_struct before hashing a new string.

2) call tc_sha256_update to hash the next string segment;
tc_sha256_update can be called as many times as needed to hash
all of the segments of a string; the order is important.

3) call tc_sha256_final to out put the digest from a hashing
operation.


---

## `int tc_sha256_init(TCSha256State_t s);`


SHA256 initialization procedure
Initializes s

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if s == NULL

- **`s`**: Sha256 state struct


---

## `int tc_sha256_update (TCSha256State_t s, const uint8_t *data, size_t datalen);`


SHA256 update procedure
Hashes data_length bytes addressed by data into state s

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if:
s == NULL,
s->iv == NULL,
data == NULL

> **Note:** Assumes s has been initialized by tc_sha256_init
@warning The state buffer 'leftover' is left in memory after processing
If your application intends to have sensitive data in this
buffer, remind to erase it after the data has been processed

- **`s`**: Sha256 state struct
- **`data`**: message to hash
- **`datalen`**: length of message to hash


---

## `int tc_sha256_final(uint8_t *digest, TCSha256State_t s);`


SHA256 final procedure
Inserts the completed hash computation into digest

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if:
s == NULL,
s->iv == NULL,
digest == NULL

> **Note:** Assumes: s has been initialized by tc_sha256_init
digest points to at least TC_SHA256_DIGEST_SIZE bytes
@warning The state buffer 'leftover' is left in memory after processing
If your application intends to have sensitive data in this
buffer, remind to erase it after the data has been processed

- **`digest`**: unsigned eight bit integer
- **`Sha256`**: state struct


---

