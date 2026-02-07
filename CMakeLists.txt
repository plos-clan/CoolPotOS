cmake_minimum_required(VERSION 3.16)
project(CoolPotOS LANGUAGES C ASM)

set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_compile_definitions(__CPOS__)
add_compile_definitions(ACPI_USE_SYSTEM_CLIBRARY)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)

include(FetchContent)

set(CMAKE_LINKER lld)
set(LIMINE_TMP_DIR ${CMAKE_CURRENT_BINARY_DIR}/limine_temp)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose build type: Debug Release RelWithDebInfo MinSizeRel")
endif ()

if (NOT DEFINED TARGET_ARCH)
    if (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        set(TARGET_ARCH x86_64)
    elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "riscv64")
        set(TARGET_ARCH riscv64)
    elseif (CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "loongarch64")
        set(TARGET_ARCH loongarch64)
    else ()
        set(TARGET_ARCH x86_64)
    endif ()
    message(STATUS "TARGET_ARCH not set, defaulting to ${TARGET_ARCH}")
else ()
    message(STATUS "Building for TARGET_ARCH=${TARGET_ARCH}")
endif ()

include(tools/bootloader.cmake)

add_subdirectory(module/e1000)
add_subdirectory(module/fatfs)
add_subdirectory(module/iso9660)

add_executable(kernel)

### 内核校验密钥生成
include(tools/gen_ecc_keys.cmake)
message(STATUS "[ECC] Public key header: ${PUB_HEADER}")
target_include_directories(kernel PRIVATE "${CMAKE_BINARY_DIR}/keys")

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(COMPILE_MODE
            -g0
            -O3
    )
else ()
    set(COMPILE_MODE
            -g3
            -O0
            -fno-omit-frame-pointer
    )
endif ()


file(GLOB_RECURSE FS_SOURCES "src/fs/*.c")
file(GLOB_RECURSE UTIL_SOURCES "src/util/*.c")
file(GLOB_RECURSE TERM_SOURCES "src/term/*.c")
file(GLOB_RECURSE DRIVER_SOURCES "src/driver/*.c")
file(GLOB_RECURSE MOD_SOURCES "src/mod/*.c")
file(GLOB_RECURSE MEM_SOURCES "src/mem/*.c")
file(GLOB_RECURSE TASK_SOURCES "src/task/*.c")
file(GLOB_RECURSE INT_SOURCES "src/int/*.c")
file(GLOB_RECURSE LIB_SOURCES "src/lib/*.c")

# Exclude ACPICA debugger/disassembler sources for kernel build.
list(FILTER LIB_SOURCES EXCLUDE REGEX "acpica/components/debugger/.*")
list(FILTER LIB_SOURCES EXCLUDE REGEX "acpica/components/disassembler/.*")
list(FILTER LIB_SOURCES EXCLUDE REGEX "acpica/components/resources/rsdump\\.c$")
list(FILTER LIB_SOURCES EXCLUDE REGEX "acpica/components/resources/rsdumpinfo\\.c$")

execute_process(
        COMMAND git rev-parse --short HEAD
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_HASH_RESULT
)

set(ISO_FILE ${CMAKE_CURRENT_BINARY_DIR}/CoolPotOS.iso)
set(IMG_FILE ${CMAKE_CURRENT_BINARY_DIR}/CoolPotOS.img)
set(ISO_DIR ${CMAKE_CURRENT_BINARY_DIR}/iso_dir)

if (GIT_HASH_RESULT EQUAL 0)
    target_compile_definitions(kernel PRIVATE "GIT_VERSION=\"${GIT_HASH}\"")
endif ()
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(kernel PRIVATE "DEBUG=1")  # 注意：原代码写反了，Debug 应该是 1
endif ()

# Architecture-specific setup
if (TARGET_ARCH STREQUAL "x86_64")
    include(tools/x86_64.cmake)
elseif (TARGET_ARCH STREQUAL "riscv64")
    include(tools/riscv64.cmake)
elseif (TARGET_ARCH STREQUAL "loongarch64")
    include(tools/loongarch64.cmake)
else ()
    message(FATAL_ERROR "Unsupported target architecture: ${TARGET_ARCH}")
endif ()
