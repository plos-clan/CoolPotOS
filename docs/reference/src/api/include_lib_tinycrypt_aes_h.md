# include/lib/tinycrypt/aes.h

## `#ifndef __TC_AES_H__ #define __TC_AES_H__ #include "types.h" #ifdef __cplusplus extern "C" {`


@file
-- Interface to an AES-128 implementation.

Overview:   AES-128 is a NIST approved block cipher specified in
FIPS 197. Block ciphers are deterministic algorithms that
perform a transformation specified by a symmetric key in fixed-
length data sets, also called blocks.

Security:   AES-128 provides approximately 128 bits of security.

Usage:      1) call tc_aes128_set_encrypt/decrypt_key to set the key.

2) call tc_aes_encrypt/decrypt to process the data.


---

## `int tc_aes128_set_encrypt_key(TCAesKeySched_t s, const uint8_t *k);`


Set AES-128 encryption key
Uses key k to initialize s

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if: s == NULL or k == NULL

> **Note:** This implementation skips the additional steps required for keys
larger than 128 bits, and must not be used for AES-192 or
AES-256 key schedule -- see FIPS 197 for details

- **`s`**: IN/OUT -- initialized struct tc_aes_key_sched_struct
- **`k`**: IN -- points to the AES key


---

## `int tc_aes_encrypt(uint8_t *out, const uint8_t *in, const TCAesKeySched_t s);`


AES-128 Encryption procedure
Encrypts contents of in buffer into out buffer under key;
schedule s

> **Note:** Assumes s was initialized by aes_set_encrypt_key;
out and in point to 16 byte buffers

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if: out == NULL or in == NULL or s == NULL

- **`out`**: IN/OUT -- buffer to receive ciphertext block
- **`in`**: IN -- a plaintext block to encrypt
- **`s`**: IN -- initialized AES key schedule


---

## `int tc_aes128_set_decrypt_key(TCAesKeySched_t s, const uint8_t *k);`


Set the AES-128 decryption key
Uses key k to initialize s

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if: s == NULL or k == NULL

> **Note:** This is the implementation of the straightforward inverse cipher
using the cipher documented in FIPS-197 figure 12, not the
equivalent inverse cipher presented in Figure 15
@warning    This routine skips the additional steps required for keys larger
than 128, and must not be used for AES-192 or AES-256 key
schedule -- see FIPS 197 for details

- **`s`**: IN/OUT -- initialized struct tc_aes_key_sched_struct
- **`k`**: IN -- points to the AES key


---

## `int tc_aes_decrypt(uint8_t *out, const uint8_t *in, const TCAesKeySched_t s);`


AES-128 Encryption procedure
Decrypts in buffer into out buffer under key schedule s

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if: out is NULL or in is NULL or s is NULL

> **Note:** Assumes s was initialized by aes_set_encrypt_key
out and in point to 16 byte buffers

- **`out`**: IN/OUT -- buffer to receive ciphertext block
- **`in`**: IN -- a plaintext block to encrypt
- **`s`**: IN -- initialized AES key schedule


---

