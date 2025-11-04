#!/bin/sh

# parse_ecc_key_v2.sh - 解析 PEM 格式的 ECC 公钥并生成 C 语言数组 (更健壮)

# ... (参数检查部分与旧脚本相同，此处略) ...

INPUT_PEM="$1"
OUTPUT_HEADER="$2"
ARRAY_NAME="$3"

# 检查依赖工具
if ! command -v openssl > /dev/null 2>&1; then
    echo "Error: openssl command not found. Please install openssl." >&2
    exit 1
fi
if ! command -v od > /dev/null 2>&1 && ! command -v hexdump > /dev/null 2>&1; then
    echo "Error: Neither 'od' nor 'hexdump' command found. Please install one of them." >&2
    exit 1
fi
if [ ! -f "$INPUT_PEM" ]; then
    echo "Error: Input file not found: $INPUT_PEM" >&2
    exit 1
fi


# -----------------------------------------------------------------------------
# 1. 使用 openssl 提取公钥的点坐标字节串（原始二进制数据）
# -----------------------------------------------------------------------------
# 步骤 a: 将 PEM 格式的公钥转换为 DER 格式的 SubjectPublicKeyInfo (SPKI) 结构
# 步骤 b: 使用 ASN.1 解析工具，找到 ECC 公钥 Point 的 OCTET STRING 的偏移量和长度，
#         然后提取该字节串。
#
# 注意：openssl 不直接提供一个提取公钥点坐标的简单命令，因此我们需要先提取
# 整个 SPKI 结构，然后手动跳过 ASN.1 头部。

# 提取整个 SubjectPublicKeyInfo (SPKI) 的 DER 编码
SPKI_DER=$(openssl pkey -pubin -in "$INPUT_PEM" -outform DER 2>/dev/null)

if [ -z "$SPKI_DER" ]; then
    echo "Error: Failed to extract DER key data from $INPUT_PEM. Check the file format." >&2
    exit 1
fi

# 以下步骤通过计算偏移量来提取公钥点坐标（未压缩：04XX...XX）
# 对于 P-256 曲线，通常需要跳过 SPKI 的 Tag, Length 和 Algorithm Identifier 部分。
# 提取原始字节串，跳过 SPKI 头部。
# 警告：这个偏移量 (通常是 27-30 字节) 依赖于曲线和编码方式。
# 更安全的方法是使用 asn1parse，但它使脚本更复杂。

# 假设公钥是标准的 P-256，头部偏移量约为 26-30 字节。
# 这是一个依赖于特定曲线的近似值。为简化，我们使用 openssl ec -pubin -text 管道提取最干净的版本。

KEY_BIN=$(openssl ec -pubin -in "$INPUT_PEM" -noout -text 2>/dev/null | \
          grep 'pub:' -A 100 | \
          grep -v 'pub:' | \
          sed 's/ //g; s/://g; s/(.*)//g' | \
          tr -d '\n')
# 再次使用 TEXT 输出，但这次我们仅使用 'tr -d' 移除所有非数字字符后的文本，并再次校验。
# 由于用户提供的错误包含文本，我们需要更强的过滤。

KEY_HEX=$(echo "$KEY_BIN" | tr -d '[:alpha:][:punct:][:space:]' | tr -d '-')

# 检查提取结果
if [ -z "$KEY_HEX" ] || [ $(( ${#KEY_HEX} % 2 )) -ne 0 ]; then
    echo "Error: Failed to cleanly extract pure HEX public key data. Check the openssl output." >&2
    exit 1
fi

KEY_LENGTH=$(( ${#KEY_HEX} / 2 ))


# -----------------------------------------------------------------------------
# 2. 转换成 C 语言数组格式 (使用 AWK 或 SHELL 字符串操作进行格式化)
# -----------------------------------------------------------------------------
# 创建或清空输出文件
echo "/*" > "$OUTPUT_HEADER"
echo " * ECC Public Key: $ARRAY_NAME" >> "$OUTPUT_HEADER"
echo " * Source File: $INPUT_PEM" >> "$OUTPUT_HEADER"
echo " * Generated on: $(date)" >> "$OUTPUT_HEADER"
echo " */" >> "$OUTPUT_HEADER"
echo "" >> "$OUTPUT_HEADER"

# 定义密钥长度
echo "#define ${ARRAY_NAME}_LEN $KEY_LENGTH" >> "$OUTPUT_HEADER"
echo "" >> "$OUTPUT_HEADER"

# 开始定义 C 数组
echo "const unsigned char $ARRAY_NAME[] = {" >> "$OUTPUT_HEADER"

# 使用 shell 循环将十六进制字符串格式化成 C 数组
I=0
while [ $I -lt $KEY_LENGTH ]; do
    BYTE_HEX=${KEY_HEX:$(($I * 2)):2}

    # 格式化输出，每 16 个字节一行
    if [ $(( $I % 16 )) -eq 0 ]; then
        echo "    " | tr -d '\n' >> "$OUTPUT_HEADER"
    fi

    # 输出字节，并在非行尾字节后加逗号
    echo "0x$BYTE_HEX" | tr -d '\n' >> "$OUTPUT_HEADER"

    # 添加逗号和换行符
    if [ $(( $I + 1 )) -lt $KEY_LENGTH ]; then
        if [ $(( ($I + 1) % 16 )) -eq 0 ]; then
            echo "," >> "$OUTPUT_HEADER"
        else
            echo ", " | tr -d '\n' >> "$OUTPUT_HEADER"
        fi
    else
        # 最后一个元素
        echo "" >> "$OUTPUT_HEADER"
    fi

    I=$(( $I + 1 ))
done

echo "};" >> "$OUTPUT_HEADER"

echo "Success: Key data extracted (${KEY_LENGTH} bytes) and written to $OUTPUT_HEADER"