# include/lib/tinycrypt/hmac_prng.h

## `#ifndef __TC_HMAC_PRNG_H__ #define __TC_HMAC_PRNG_H__ #include <lib/tinycrypt/sha256.h> #include <lib/tinycrypt/hmac.h> #ifdef __cplusplus extern "C" {`


@file
Interface to an HMAC-PRNG implementation.

Overview:   A pseudo-random number generator (PRNG) generates a sequence
of numbers that have a distribution close to the one expected
for a sequence of truly random numbers. The NIST Special
Publication 800-90A specifies several mechanisms to generate
sequences of pseudo random numbers, including the HMAC-PRNG one
which is based on HMAC. TinyCrypt implements HMAC-PRNG with
certain modifications from the NIST SP 800-90A spec.

Security:   A cryptographically secure PRNG depends on the existence of an
entropy source to provide a truly random seed as well as the
security of the primitives used as the building blocks (HMAC and
SHA256, for TinyCrypt).

The NIST SP 800-90A standard tolerates a null personalization,
while TinyCrypt requires a non-null personalization. This is
because a personalization string (the host name concatenated
with a time stamp, for example) is easily computed and might be
the last line of defense against failure of the entropy source.

Requires:   - SHA-256
- HMAC

Usage:      1) call tc_hmac_prng_init to set the HMAC key and process the
personalization data.

2) call tc_hmac_prng_reseed to process the seed and additional
input.

3) call tc_hmac_prng_generate to out put the pseudo-random data.


---

## `int tc_hmac_prng_init(TCHmacPrng_t prng, const uint8_t *personalization, unsigned int plen);`


HMAC-PRNG initialization procedure
Initializes prng with personalization, disables tc_hmac_prng_generate

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if:
prng == NULL,
personalization == NULL,
plen > MAX_PLEN

> **Note:** Assumes: - personalization != NULL.
The personalization is a platform unique string (e.g., the host
name) and is the last line of defense against failure of the
entropy source
@warning    NIST SP 800-90A specifies 3 items as seed material during
initialization: entropy seed, personalization, and an optional
nonce. TinyCrypts requires instead a non-null personalization
(which is easily computed) and indirectly requires an entropy
seed (since the reseed function is mandatorily called after
init)

- **`prng`**: IN/OUT -- the PRNG state to initialize
- **`personalization`**: IN -- personalization string
- **`plen`**: IN -- personalization length in bytes


---

## `int tc_hmac_prng_reseed(TCHmacPrng_t prng, const uint8_t *seed, unsigned int seedlen, const uint8_t *additional_input, unsigned int additionallen);`


HMAC-PRNG reseed procedure
Mixes seed into prng, enables tc_hmac_prng_generate

- **Returns**: returns  TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if:
prng == NULL,
seed == NULL,
seedlen < MIN_SLEN,
seendlen > MAX_SLEN,
additional_input != (const uint8_t *) 0 && additionallen == 0,
additional_input != (const uint8_t *) 0 && additionallen > MAX_ALEN

> **Note:** Assumes:- tc_hmac_prng_init has been called for prng
- seed has sufficient entropy.


- **`prng`**: IN/OUT -- the PRNG state
- **`seed`**: IN -- entropy to mix into the prng
- **`seedlen`**: IN -- length of seed in bytes
- **`additional_input`**: IN -- additional input to the prng
- **`additionallen`**: IN -- additional input length in bytes


---

## `int tc_hmac_prng_generate(uint8_t *out, unsigned int outlen, TCHmacPrng_t prng);`


HMAC-PRNG generate procedure
Generates outlen pseudo-random bytes into out buffer, updates prng

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_HMAC_PRNG_RESEED_REQ (-1) if a reseed is needed
returns TC_CRYPTO_FAIL (0) if:
out == NULL,
prng == NULL,
outlen == 0,
outlen >= MAX_OUT

> **Note:** Assumes tc_hmac_prng_init has been called for prng

- **`out`**: IN/OUT -- buffer to receive output
- **`outlen`**: IN -- size of out buffer in bytes
- **`prng`**: IN/OUT -- the PRNG state


---

