#!/usr/bin/sh

find src/ libs/ \
    -path "src/lib/acpica" -prune -o \
    -path "src/include/lib/acpica" -prune -o \
    -type f \( -name "*.c" -o -name "*.h" \) \
    -exec clang-format -i {} +

echo "Formatting done! Meow~"
