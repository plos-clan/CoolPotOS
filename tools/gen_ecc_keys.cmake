set(KEY_DIR "${CMAKE_BINARY_DIR}/keys")
set(PRIV_KEY "${KEY_DIR}/module_signing_priv.pem")
set(PUB_KEY  "${KEY_DIR}/module_signing_pub.pem")
set(PUB_HEADER "${KEY_DIR}/pubkey.h")

file(MAKE_DIRECTORY "${KEY_DIR}")

# 生成密钥（如果不存在）
if (NOT EXISTS "${PRIV_KEY}")
    message(STATUS "[ECC] Generating new ECC key pair...")
    execute_process(
            COMMAND openssl ecparam -name prime256v1 -genkey -noout -out "${PRIV_KEY}"
            RESULT_VARIABLE RES1
    )
    if (NOT RES1 EQUAL 0)
        message(FATAL_ERROR "Failed to generate ECC private key.")
    endif()

    execute_process(
            COMMAND openssl ec -in "${PRIV_KEY}" -pubout -out "${PUB_KEY}"
            RESULT_VARIABLE RES2
    )
    if (NOT RES2 EQUAL 0)
        message(FATAL_ERROR "Failed to export ECC public key.")
    endif()
else()
    message(STATUS "[ECC] Using existing ECC key: ${PRIV_KEY}")
endif()

# 生成原始数组数据 (只取末尾65字节)
set(TMP_BIN "${KEY_DIR}/pubkey_raw.h")
execute_process(
        COMMAND bash -c "openssl ec -pubin -in '${PUB_KEY}' -outform DER | tail -c 65 | xxd -i > '${TMP_BIN}'"
        RESULT_VARIABLE RES3
)
if (NOT RES3 EQUAL 0)
    message(FATAL_ERROR "Failed to generate raw public key data.")
endif()

# 读取 xxd -i 输出内容
file(READ "${TMP_BIN}" RAW_HEX)

# 将变量名改成 cpos_signing_key_pub
string(REPLACE "unsigned char" "const unsigned char" RAW_HEX "${RAW_HEX}")
string(REPLACE "pubkey_raw_bin" "cpos_signing_key_pub" RAW_HEX "${RAW_HEX}")
string(REPLACE "unsigned int cpos_signing_key_pub_len" "#define cpos_signing_key_pub_LEN" RAW_HEX "${RAW_HEX}")

# 写入带注释的头文件
file(WRITE "${PUB_HEADER}" "/*\n")
file(APPEND "${PUB_HEADER}" " * ECC Public Key: cpos_signing_key_pub\n")
file(APPEND "${PUB_HEADER}" " * Source File: ${PUB_KEY}\n")
file(APPEND "${PUB_HEADER}" " * Generated on: ${CMAKE_TIME}\n")
file(APPEND "${PUB_HEADER}" " */\n\n")
file(APPEND "${PUB_HEADER}" " uint8_t cpos_signing_key_pub[] = {\n")
file(APPEND "${PUB_HEADER}" "${RAW_HEX}")
file(APPEND "${PUB_HEADER}" "};\n")

# 清理临时文件
file(REMOVE "${TMP_BIN}")

message(STATUS "[ECC] ECC public key exported to ${PUB_HEADER}")
