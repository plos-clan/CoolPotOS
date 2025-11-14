# include/lib/tinycrypt/ctr_mode.h

## `#ifndef __TC_CTR_MODE_H__ #define __TC_CTR_MODE_H__ #include <lib/tinycrypt/aes.h> #include <lib/tinycrypt/constants.h> #ifdef __cplusplus extern "C" {`


@file
Interface to CTR mode.

Overview:  CTR (pronounced "counter") mode is a NIST approved mode of
operation defined in SP 800-38a. It can be used with any
block cipher to provide confidentiality of strings of any
length. TinyCrypt hard codes AES128 as the block cipher.

Security:  CTR mode achieves confidentiality only if the counter value is
never reused with a same encryption key. If the counter is
repeated, than an adversary might be able to defeat the scheme.

A usual method to ensure different counter values refers to
initialize the counter in a given value (0, for example) and
increases it every time a new block is enciphered. This naturally
leaves to a limitation on the number q of blocks that can be
enciphered using a same key: q < 2^(counter size).

TinyCrypt uses a counter of 32 bits. This means that after 2^32
block encryptions, the counter will be reused (thus losing CBC
security). 2^32 block encryptions should be enough for most of
applications targeting constrained devices. Applications intended
to encrypt a larger number of blocks must replace the key after
2^32 block encryptions.

CTR mode provides NO data integrity.

Requires: AES-128

Usage:     1) call tc_ctr_mode to process the data to encrypt/decrypt.



---

## `int tc_ctr_mode(uint8_t *out, unsigned int outlen, const uint8_t *in, unsigned int inlen, uint8_t *ctr, const TCAesKeySched_t sched);`


CTR mode encryption/decryption procedure.
CTR mode encrypts (or decrypts) inlen bytes from in buffer into out buffer

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if:
out == NULL or
in == NULL or
ctr == NULL or
sched == NULL or
inlen == 0 or
outlen == 0 or
inlen != outlen

> **Note:** Assumes:- The current value in ctr has NOT been used with sched
- out points to inlen bytes
- in points to inlen bytes
- ctr is an integer counter in littleEndian format
- sched was initialized by aes_set_encrypt_key

- **`out`**: OUT -- produced ciphertext (plaintext)
- **`outlen`**: IN -- length of ciphertext buffer in bytes
- **`in`**: IN -- data to encrypt (or decrypt)
- **`inlen`**: IN -- length of input data in bytes
- **`ctr`**: IN/OUT -- the current counter value
- **`sched`**: IN -- an initialized AES key schedule


---

