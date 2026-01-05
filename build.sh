set -e

cmake -S . -B build/ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DTARGET_ARCH=x86_64

cmake --build build/ --target run -j 12

cp build/compile_commands.json compile_commands.json
