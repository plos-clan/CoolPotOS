# include/lib/tinycrypt/ccm_mode.h

## `#ifndef __TC_CCM_MODE_H__ #define __TC_CCM_MODE_H__ #include <lib/tinycrypt/aes.h> #include <stddef.h> #ifdef __cplusplus extern "C" {`


@file
Interface to a CCM mode implementation.

Overview: CCM (for "Counter with CBC-MAC") mode is a NIST approved mode of
operation defined in SP 800-38C.

TinyCrypt CCM implementation accepts:

1) Both non-empty payload and associated data (it encrypts and
authenticates the payload and also authenticates the associated
data);
2) Non-empty payload and empty associated data (it encrypts and
authenticates the payload);
3) Non-empty associated data and empty payload (it degenerates to
an authentication mode on the associated data).

TinyCrypt CCM implementation accepts associated data of any length
between 0 and (2^16 - 2^8) bytes.

Security: The mac length parameter is an important parameter to estimate the
security against collision attacks (that aim at finding different
messages that produce the same authentication tag). TinyCrypt CCM
implementation accepts any even integer between 4 and 16, as
suggested in SP 800-38C.

RFC-3610, which also specifies CCM, presents a few relevant
security suggestions, such as: it is recommended for most
applications to use a mac length greater than 8. Besides, the
usage of the same nonce for two different messages which are
encrypted with the same key destroys the security of CCM mode.

Requires: AES-128

Usage:    1) call tc_ccm_config to configure.

2) call tc_ccm_mode_encrypt to encrypt data and generate tag.

3) call tc_ccm_mode_decrypt to decrypt data and verify tag.


---

## `int tc_ccm_config(TCCcmMode_t c, TCAesKeySched_t sched, uint8_t *nonce, unsigned int nlen, unsigned int mlen);`


CCM configuration procedure

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if:
c == NULL or
sched == NULL or
nonce == NULL or
mlen != {4, 6, 8, 10, 12, 16}

- **`c`**: -- CCM state
- **`sched`**: IN -- AES key schedule
- **`nonce`**: IN - nonce
- **`nlen`**: -- nonce length in bytes
- **`mlen`**: -- mac length in bytes (parameter t in SP-800 38C)


---

## `int tc_ccm_generation_encryption(uint8_t *out, unsigned int olen, const uint8_t *associated_data, unsigned int alen, const uint8_t *payload, unsigned int plen, TCCcmMode_t c);`


CCM tag generation and encryption procedure

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if:
out == NULL or
c == NULL or
((plen > 0) and (payload == NULL)) or
((alen > 0) and (associated_data == NULL)) or
(alen >= TC_CCM_AAD_MAX_BYTES) or
(plen >= TC_CCM_PAYLOAD_MAX_BYTES) or
(olen < plen + maclength)


- **`out`**: OUT -- encrypted data
- **`olen`**: IN -- output length in bytes
- **`associated_data`**: IN -- associated data
- **`alen`**: IN -- associated data length in bytes
- **`payload`**: IN -- payload
- **`plen`**: IN -- payload length in bytes
- **`c`**: IN -- CCM state


> **Note:** : out buffer should be at least (plen + c->mlen) bytes long.


> **Note:** : The sequence b for encryption is formatted as follows:
b = [FLAGS | nonce | counter ], where:
FLAGS is 1 byte long
nonce is 13 bytes long
counter is 2 bytes long
The byte FLAGS is composed by the following 8 bits:
0-2 bits: used to represent the value of q-1
3-7 btis: always 0's


> **Note:** : The sequence b for authentication is formatted as follows:
b = [FLAGS | nonce | length(mac length)], where:
FLAGS is 1 byte long
nonce is 13 bytes long
length(mac length) is 2 bytes long
The byte FLAGS is composed by the following 8 bits:
0-2 bits: used to represent the value of q-1
3-5 bits: mac length (encoded as: (mlen-2)/2)
6: Adata (0 if alen == 0, and 1 otherwise)
7: always 0


---

## `int tc_ccm_decryption_verification(uint8_t *out, unsigned int olen, const uint8_t *associated_data, unsigned int alen, const uint8_t *payload, unsigned int plen, TCCcmMode_t c);`


CCM decryption and tag verification procedure

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if:
out == NULL or
c == NULL or
((plen > 0) and (payload == NULL)) or
((alen > 0) and (associated_data == NULL)) or
(alen >= TC_CCM_AAD_MAX_BYTES) or
(plen >= TC_CCM_PAYLOAD_MAX_BYTES) or
(olen < plen - c->mlen)


- **`out`**: OUT -- decrypted data
- **`associated_data`**: IN -- associated data
- **`alen`**: IN -- associated data length in bytes
- **`payload`**: IN -- payload
- **`plen`**: IN -- payload length in bytes
- **`c`**: IN -- CCM state


> **Note:** : out buffer should be at least (plen - c->mlen) bytes long.


> **Note:** : The sequence b for encryption is formatted as follows:
b = [FLAGS | nonce | counter ], where:
FLAGS is 1 byte long
nonce is 13 bytes long
counter is 2 bytes long
The byte FLAGS is composed by the following 8 bits:
0-2 bits: used to represent the value of q-1
3-7 btis: always 0's


> **Note:** : The sequence b for authentication is formatted as follows:
b = [FLAGS | nonce | length(mac length)], where:
FLAGS is 1 byte long
nonce is 13 bytes long
length(mac length) is 2 bytes long
The byte FLAGS is composed by the following 8 bits:
0-2 bits: used to represent the value of q-1
3-5 bits: mac length (encoded as: (mlen-2)/2)
6: Adata (0 if alen == 0, and 1 otherwise)
7: always 0


---

