# include/lib/tinycrypt/ecc_dsa.h

## `#ifndef __TC_ECC_DSA_H__ #define __TC_ECC_DSA_H__ #include <lib/tinycrypt/ecc.h> #ifdef __cplusplus extern "C" {`


@file
-- Interface to EC-DSA implementation.

Overview: This software is an implementation of EC-DSA. This implementation
uses curve NIST p-256.

Security: The curve NIST p-256 provides approximately 128 bits of security.

Usage:  - To sign: Compute a hash of the data you wish to sign (SHA-2 is
recommended) and pass it in to ecdsa_sign function along with your
private key and a random number. You must use a new non-predictable
random number to generate each new signature.
- To verify a signature: Compute the hash of the signed data using
the same hash as the signer and pass it to this function along with
the signer's public key and the signature values (r and s).


---

## `int uECC_sign(const uint8_t *p_private_key, const uint8_t *p_message_hash, unsigned p_hash_size, uint8_t *p_signature, uECC_Curve curve);`


Generate an ECDSA signature for a given hash value.

- **Returns**: returns TC_CRYPTO_SUCCESS (1) if the signature generated successfully
returns TC_CRYPTO_FAIL (0) if an error occurred.


- **`p_private_key`**: IN -- Your private key.
- **`p_message_hash`**: IN -- The hash of the message to sign.
- **`p_hash_size`**: IN -- The size of p_message_hash in bytes.
- **`p_signature`**: OUT -- Will be filled in with the signature value. Must be
at least 2 * curve size long (for secp256r1, signature must be 64 bytes long).

@warning A cryptographically-secure PRNG function must be set (using
uECC_set_rng()) before calling uECC_sign().

> **Note:** Usage: Compute a hash of the data you wish to sign (SHA-2 is
recommended) and pass it in to this function along with your private key.

> **Note:** side-channel countermeasure: algorithm strengthened against timing
attack.


---

## `int uECC_verify(const uint8_t *p_public_key, const uint8_t *p_message_hash, unsigned int p_hash_size, const uint8_t *p_signature, uECC_Curve curve);`


Verify an ECDSA signature.

- **Returns**: returns TC_SUCCESS (1) if the signature is valid
returns TC_FAIL (0) if the signature is invalid.


- **`p_public_key`**: IN -- The signer's public key.
- **`p_message_hash`**: IN -- The hash of the signed data.
- **`p_hash_size`**: IN -- The size of p_message_hash in bytes.
- **`p_signature`**: IN -- The signature values.


> **Note:** Usage: Compute the hash of the signed data using the same hash as the
signer and pass it to this function along with the signer's public key and
the signature values (hash_size and signature).


---

