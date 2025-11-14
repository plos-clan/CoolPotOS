# include/lib/tinycrypt/utils.h

## `#ifndef __TC_UTILS_H__ #define __TC_UTILS_H__ #include "types.h" #include "krlibc.h" #ifdef __cplusplus extern "C" {`


@file
Interface to platform-dependent run-time operations.



---

## `unsigned int _copy(uint8_t *to, unsigned int to_len, const uint8_t *from, unsigned int from_len);`


Copy the the buffer 'from' to the buffer 'to'.

- **Returns**: returns TC_CRYPTO_SUCCESS (1)
returns TC_CRYPTO_FAIL (0) if:
from_len > to_len.


- **`to`**: OUT -- destination buffer
- **`to_len`**: IN -- length of destination buffer
- **`from`**: IN -- origin buffer
- **`from_len`**: IN -- length of origin buffer


---

## `void _set(void *to, uint8_t val, unsigned int len);`


Set the value 'val' into the buffer 'to', 'len' times.


- **`to`**: OUT -- destination buffer
- **`val`**: IN -- value to be set in 'to'
- **`len`**: IN -- number of times the value will be copied


---

## `#ifdef TINYCRYPT_ARCH_HAS_SET_SECURE extern void _set_secure(void *to, uint8_t val, unsigned int len);`


Set the value 'val' into the buffer 'to', 'len' times, in a way
which does not risk getting optimized out by the compiler
In cases where the compiler does not set __GNUC__ and where the
optimization level removes the memset, it may be necessary to
implement a _set_secure function and define the
TINYCRYPT_ARCH_HAS_SET_SECURE, which then can ensure that the
memset does not get optimized out.


- **`to`**: OUT -- destination buffer
- **`val`**: IN -- value to be set in 'to'
- **`len`**: IN -- number of times the value will be copied


---

