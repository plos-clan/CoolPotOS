#!/usr/bin/env bash
# sign_module.sh - V17 (robust DER->R||S extraction, append 72-byte CPOS sig block)
# Usage: sign_module.sh <private_key.pem> <module_file>

set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <private_key.pem> <module_file>" >&2
    exit 1
fi

PRIVATE_KEY="$1"
MODULE_FILE="$2"
SIGNATURE_FILE="${MODULE_FILE}.sig"

if [ ! -f "$PRIVATE_KEY" ]; then
    echo "Error: private key not found: $PRIVATE_KEY" >&2
    exit 1
fi
if [ ! -f "$MODULE_FILE" ]; then
    echo "Error: module file not found: $MODULE_FILE" >&2
    exit 1
fi

openssl dgst -sha256 -sign private.pem -out "$SIGNATURE_FILE" "$MODULE_FILE"
