#!/usr/bin/sh

find src/ libs/ -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -exec clang-format -i {} +
