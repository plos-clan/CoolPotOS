#include "mod/modchk.h"
#include "term/klog.h"

#if MODULE_CHECK
#    include "lib/tinycrypt/constants.h"
#    include "lib/tinycrypt/ecc.h"
#    include "lib/tinycrypt/ecc_dsa.h"
#    include "lib/tinycrypt/sha256.h"
#    include "pubkey.h" // 该文件自动生成 (包含 cpos_signing_key_pub[])
#endif

bool mod_check_signature(module_t *mod, const uint8_t *module_buffer, size_t module_size) {
#if !(MODULE_CHECK)
    return true;
#else
    if (module_size < sizeof(struct module_signature)) {
        kerror("module file too small to contain signature info.");
        return false;
    }
    size_t data_len_to_hash = module_size - sizeof(struct module_signature);
    const struct module_signature *sig_info =
        (const struct module_signature *)(module_buffer + data_len_to_hash);
    if (sig_info->magic != CPOS_SIG_MAGIC) {
        kerror("invalid signature magic 0x%X. Unsigned or corrupted module.", sig_info->magic);
        return false;
    }
    if (sig_info->hash_algo != HASH_SHA256) {
        kerror("unsupported hash algorithm: %u.", sig_info->hash_algo);
        return false;
    }
    if (sig_info->sig_len != ECC_SIG_LEN) {
        kerror(
            "invalid signature length: %u. Expected %u for ECC P-256.",
            sig_info->sig_len,
            ECC_SIG_LEN
        );
        return false;
    }
    uint8_t calculated_hash[HASH_LEN];
    struct tc_sha256_state_struct s;
    if (tc_sha256_init(&s) != TC_CRYPTO_SUCCESS) {
        kerror("SHA256 initialization failed.");
        return false;
    }
    if (tc_sha256_update(&s, module_buffer, data_len_to_hash) != TC_CRYPTO_SUCCESS) {
        kerror("SHA256 update failed.");
        return false;
    }
    if (tc_sha256_final(calculated_hash, &s) != TC_CRYPTO_SUCCESS) {
        kerror("SHA256 finalization failed.");
        return false;
    }

    const uint8_t *signature_data = sig_info->signature; // R || S (64 bytes)

    // cpos_signing_key_pub[] 是硬编码的公钥 (X || Y, 64 bytes)

    const uint8_t *pub = cpos_signing_key_pub;

    // 去除 0x04 || X || Y (65), 保留原始签名头
    if (pub[0] == 0x04) {
        pub += 1;
    }

    // uECC_verify(public_key, hash, hash_len, signature, curve)
    int result = uECC_verify(
        pub,             // 公钥 (X || Y)
        calculated_hash, // 哈希值
        HASH_LEN,        // 哈希长度 (32)
        signature_data,  // 签名 (R || S)
        uECC_secp256r1() // P-256 曲线上下文
    );

    // uECC_verify 成功返回 1，失败返回 0
    if (result == 1) {
        return true;
    } else {
        kerror(
            "%s: module signature verification failed (err=%d): signature not trusted.",
            mod->path,
            result
        );
        return false;
    }
#endif
}