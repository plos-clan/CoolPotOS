# include/lib/tinycrypt/ctr_prng.h

## `#ifndef __TC_CTR_PRNG_H__ #define __TC_CTR_PRNG_H__ #include <lib/tinycrypt/aes.h> #define TC_CTR_PRNG_RESEED_REQ -1 #ifdef __cplusplus extern "C" {`


@file
Interface to a CTR-PRNG implementation.

Overview:   A pseudo-random number generator (PRNG) generates a sequence
of numbers that have a distribution close to the one expected
for a sequence of truly random numbers. The NIST Special
Publication 800-90A specifies several mechanisms to generate
sequences of pseudo random numbers, including the CTR-PRNG one
which is based on AES. TinyCrypt implements CTR-PRNG with
AES-128.

Security:   A cryptographically secure PRNG depends on the existence of an
entropy source to provide a truly random seed as well as the
security of the primitives used as the building blocks (AES-128
in this instance).

Requires:   - AES-128

Usage:      1) call tc_ctr_prng_init to seed the prng context

2) call tc_ctr_prng_reseed to mix in additional entropy into
the prng context

3) call tc_ctr_prng_generate to output the pseudo-random data

4) call tc_ctr_prng_uninstantiate to zero out the prng context


---

## `int tc_ctr_prng_init(TCCtrPrng_t * const ctx, uint8_t const * const entropy, unsigned int entropyLen, uint8_t const * const personalization, unsigned int pLen);`


CTR-PRNG initialization procedure
Initializes prng context with entropy and personalization string (if any)

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if:
ctx == NULL,
entropy == NULL,
entropyLen < (TC_AES_KEY_SIZE + TC_AES_BLOCK_SIZE)

> **Note:** Only the first (TC_AES_KEY_SIZE + TC_AES_BLOCK_SIZE) bytes of
both the entropy and personalization inputs are used -
supplying additional bytes has no effect.

- **`ctx`**: IN/OUT -- the PRNG context to initialize
- **`entropy`**: IN -- entropy used to seed the PRNG
- **`entropyLen`**: IN -- entropy length in bytes
- **`personalization`**: IN -- personalization string used to seed the PRNG
(may be null)

- **`plen`**: IN -- personalization length in bytes



---

## `int tc_ctr_prng_reseed(TCCtrPrng_t * const ctx, uint8_t const * const entropy, unsigned int entropyLen, uint8_t const * const additional_input, unsigned int additionallen);`


CTR-PRNG reseed procedure
Mixes entropy and additional_input into the prng context

- **Returns**: returns  TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if:
ctx == NULL,
entropy == NULL,
entropylen < (TC_AES_KEY_SIZE + TC_AES_BLOCK_SIZE)

> **Note:** It is better to reseed an existing prng context rather than
re-initialise, so that any existing entropy in the context is
presereved.  This offers some protection against undetected failures
of the entropy source.

> **Note:** Assumes tc_ctr_prng_init has been called for ctx

- **`ctx`**: IN/OUT -- the PRNG state
- **`entropy`**: IN -- entropy to mix into the prng
- **`entropylen`**: IN -- length of entropy in bytes
- **`additional_input`**: IN -- additional input to the prng (may be null)
- **`additionallen`**: IN -- additional input length in bytes


---

## `int tc_ctr_prng_generate(TCCtrPrng_t * const ctx, uint8_t const * const additional_input, unsigned int additionallen, uint8_t * const out, unsigned int outlen);`


CTR-PRNG generate procedure
Generates outlen pseudo-random bytes into out buffer, updates prng

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CTR_PRNG_RESEED_REQ (-1) if a reseed is needed
returns TC_CRYPTO_FAIL (0) if:
ctx == NULL,
out == NULL,
outlen >= 2^16

> **Note:** Assumes tc_ctr_prng_init has been called for ctx

- **`ctx`**: IN/OUT -- the PRNG context
- **`additional_input`**: IN -- additional input to the prng (may be null)
- **`additionallen`**: IN -- additional input length in bytes
- **`out`**: IN/OUT -- buffer to receive output
- **`outlen`**: IN -- size of out buffer in bytes


---

## `void tc_ctr_prng_uninstantiate(TCCtrPrng_t * const ctx);`


CTR-PRNG uninstantiate procedure
Zeroes the internal state of the supplied prng context

- **Returns**: none
- **`ctx`**: IN/OUT -- the PRNG context


---

