# include/lib/tinycrypt/cmac_mode.h

## `#ifndef __TC_CMAC_MODE_H__ #define __TC_CMAC_MODE_H__ #include <lib/tinycrypt/aes.h> #include <stddef.h> #ifdef __cplusplus extern "C" {`


@file
Interface to a CMAC implementation.

Overview: CMAC is defined NIST in SP 800-38B, and is the standard algorithm
for computing a MAC using a block cipher. It can compute the MAC
for a byte string of any length. It is distinguished from CBC-MAC
in the processing of the final message block; CMAC uses a
different technique to compute the final message block is full
size or only partial, while CBC-MAC uses the same technique for
both. This difference permits CMAC to be applied to variable
length messages, while all messages authenticated by CBC-MAC must
be the same length.

Security: AES128-CMAC mode of operation offers 64 bits of security against
collision attacks. Note however that an external attacker cannot
generate the tags him/herself without knowing the MAC key. In this
sense, to attack the collision property of AES128-CMAC, an
external attacker would need the cooperation of the legal user to
produce an exponentially high number of tags (e.g. 2^64) to
finally be able to look for collisions and benefit from them. As
an extra precaution, the current implementation allows to at most
2^48 calls to the tc_cmac_update function before re-calling
tc_cmac_setup (allowing a new key to be set), as suggested in
Appendix B of SP 800-38B.

Requires: AES-128

Usage:   This implementation provides a "scatter-gather" interface, so that
the CMAC value can be computed incrementally over a message
scattered in different segments throughout memory. Experience shows
this style of interface tends to minimize the burden of programming
correctly. Like all symmetric key operations, it is session
oriented.

To begin a CMAC session, use tc_cmac_setup to initialize a struct
tc_cmac_struct with encryption key and buffer. Our implementation
always assume that the AES key to be the same size as the block
cipher block size. Once setup, this data structure can be used for
many CMAC computations.

Once the state has been setup with a key, computing the CMAC of
some data requires three steps:

(1) first use tc_cmac_init to initialize a new CMAC computation.
(2) next mix all of the data into the CMAC computation state using
tc_cmac_update. If all of the data resides in a single data
segment then only one tc_cmac_update call is needed; if data
is scattered throughout memory in n data segments, then n calls
will be needed. CMAC IS ORDER SENSITIVE, to be able to detect
attacks that swap bytes, so the order in which data is mixed
into the state is critical!
(3) Once all of the data for a message has been mixed, use
tc_cmac_final to compute the CMAC tag value.

Steps (1)-(3) can be repeated as many times as you want to CMAC
multiple messages. A practical limit is 2^48 1K messages before you
have to change the key.

Once you are done computing CMAC with a key, it is a good idea to
destroy the state so an attacker cannot recover the key; use
tc_cmac_erase to accomplish this.


---

## `int tc_cmac_setup(TCCmacState_t s, const uint8_t *key, TCAesKeySched_t sched);`


Configures the CMAC state to use the given AES key

- **Returns**: returns TC_CRYPTO_SUCCESS (1) after having configured the CMAC state
returns TC_CRYPTO_FAIL (0) if:
s == NULL or
key == NULL


- **`s`**: IN/OUT -- the state to set up
- **`key`**: IN -- the key to use
- **`sched`**: IN -- AES key schedule


---

## `int tc_cmac_erase(TCCmacState_t s);`


Erases the CMAC state

- **Returns**: returns TC_CRYPTO_SUCCESS (1) after having configured the CMAC state
returns TC_CRYPTO_FAIL (0) if:
s == NULL


- **`s`**: IN/OUT -- the state to erase


---

## `int tc_cmac_init(TCCmacState_t s);`


Initializes a new CMAC computation

- **Returns**: returns TC_CRYPTO_SUCCESS (1) after having initialized the CMAC state
returns TC_CRYPTO_FAIL (0) if:
s == NULL


- **`s`**: IN/OUT -- the state to initialize


---

## `int tc_cmac_update(TCCmacState_t s, const uint8_t *data, size_t dlen);`


Incrementally computes CMAC over the next data segment

- **Returns**: returns TC_CRYPTO_SUCCESS (1) after successfully updating the CMAC state
returns TC_CRYPTO_FAIL (0) if:
s == NULL or
if data == NULL when dlen > 0


- **`s`**: IN/OUT -- the CMAC state
- **`data`**: IN -- the next data segment to MAC
- **`dlen`**: IN -- the length of data in bytes


---

## `int tc_cmac_final(uint8_t *tag, TCCmacState_t s);`


Generates the tag from the CMAC state

- **Returns**: returns TC_CRYPTO_SUCCESS (1) after successfully generating the tag
returns TC_CRYPTO_FAIL (0) if:
tag == NULL or
s == NULL


- **`tag`**: OUT -- the CMAC tag
- **`s`**: IN -- CMAC state


---

