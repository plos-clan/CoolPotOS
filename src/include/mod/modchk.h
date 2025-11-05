#pragma once

#define CPOS_SIG_MAGIC 0x43504F53 // ASCII "CPOS" (0x43='C',0x50='P',0x4F='O',0x53='S')

#define HASH_SHA256     1
#define SHA256_HASH_LEN 32
#define HASH_LEN        SHA256_HASH_LEN

#define ECC_KEY_LEN    32                // P-256 field element length in bytes
#define ECC_PUBKEY_LEN (2 * ECC_KEY_LEN) // X||Y = 64 bytes
#define ECC_SIG_LEN    (2 * ECC_KEY_LEN) // R||S = 64 bytes

#include "metadata.h"
#include "module.h"
#include "types.h"

struct module_signature {
    uint32_t magic;                  // CPOS_SIG_MAGIC
    uint8_t  hash_algo;              // HASH_SHA256
    uint8_t  sig_len;                // ECC_SIG_LEN
    uint8_t  reserved[2];            // 对齐/保留
    uint8_t  signature[ECC_SIG_LEN]; // R||S (64 bytes)
} __attribute__((packed));           // 确保结构体没有填充

bool mod_check_signature(module_t *mod,const uint8_t *module_buffer, size_t module_size);
