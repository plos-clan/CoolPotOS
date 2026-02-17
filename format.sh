#!/usr/bin/sh

find src/ libs/ \
    -path "src/lib" -prune -o \
    -path "src/include/lib" -prune -o \
    -type f \( -name "*.c" -o -name "*.h" \) \
    -exec clang-format -i {} +

echo "Formatting done! Meow~"
