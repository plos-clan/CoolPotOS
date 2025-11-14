# include/lib/tinycrypt/ecc_dh.h

## `#ifndef __TC_ECC_DH_H__ #define __TC_ECC_DH_H__ #include <lib/tinycrypt/ecc.h> #ifdef __cplusplus extern "C" {`


@file
-- Interface to EC-DH implementation.

Overview: This software is an implementation of EC-DH. This implementation
uses curve NIST p-256.

Security: The curve NIST p-256 provides approximately 128 bits of security.


---

## `int uECC_make_key(uint8_t *p_public_key, uint8_t *p_private_key, uECC_Curve curve);`


Create a public/private key pair.

- **Returns**: returns TC_CRYPTO_SUCCESS (1) if the key pair was generated successfully
returns TC_CRYPTO_FAIL (0) if error while generating key pair


- **`p_public_key`**: OUT -- Will be filled in with the public key. Must be at
least 2 * the curve size (in bytes) long. For curve secp256r1, p_public_key
must be 64 bytes long.

- **`p_private_key`**: OUT -- Will be filled in with the private key. Must be as
long as the curve order (for secp256r1, p_private_key must be 32 bytes long).


> **Note:** side-channel countermeasure: algorithm strengthened against timing
attack.
@warning A cryptographically-secure PRNG function must be set (using
uECC_set_rng()) before calling uECC_make_key().


---

## `int uECC_make_key_with_d(uint8_t *p_public_key, uint8_t *p_private_key, unsigned int *d, uECC_Curve curve);`


Create a public/private key pair given a specific d.


> **Note:** THIS FUNCTION SHOULD BE CALLED ONLY FOR TEST PURPOSES. Refer to
uECC_make_key() function for real applications.


---

## `int uECC_shared_secret(const uint8_t *p_public_key, const uint8_t *p_private_key, uint8_t *p_secret, uECC_Curve curve);`


Compute a shared secret given your secret key and someone else's
public key.

- **Returns**: returns TC_CRYPTO_SUCCESS (1) if the shared secret was computed successfully
returns TC_CRYPTO_FAIL (0) otherwise


- **`p_secret`**: OUT -- Will be filled in with the shared secret value. Must be
the same size as the curve size (for curve secp256r1, secret must be 32 bytes
long.

- **`p_public_key`**: IN -- The public key of the remote party.
- **`p_private_key`**: IN -- Your private key.

@warning It is recommended to use the output of uECC_shared_secret() as the
input of a recommended Key Derivation Function (see NIST SP 800-108) in
order to produce a cryptographically secure symmetric key.


---

