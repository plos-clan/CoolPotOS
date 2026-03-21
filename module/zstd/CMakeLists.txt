set(ZSTD_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common/debug.c
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common/entropy_common.c
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common/error_private.c
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common/fse_decompress.c
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common/xxhash.c
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/common/zstd_common.c
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/decompress/huf_decompress.c
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/decompress/zstd_ddict.c
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/decompress/zstd_decompress.c
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd/decompress/zstd_decompress_block.c
        zstd_module.c
)

add_library(zstd SHARED ${ZSTD_SOURCES})

set_target_properties(zstd PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/kmod
)

target_include_directories(zstd PUBLIC
        ${CMAKE_SOURCE_DIR}/module/all_include
        ${CMAKE_SOURCE_DIR}/src/include
        ${CMAKE_CURRENT_SOURCE_DIR}/zstd
)

target_compile_definitions(zstd PRIVATE
        __CPOS_MODULE__
        DEBUGLEVEL=0
        ZSTD_TRACE=0
        ZSTD_HEAPMODE=0
        ZSTD_DISABLE_ASM
        ZSTD_NO_INTRINSICS
)

target_apply_kernel_module_options(zstd)

target_compile_options(zstd PRIVATE
        -Wno-unused-function
        -Wno-unused-parameter
)

target_link_options(zstd PRIVATE
        -nostdlib
        -Wl,-Bsymbolic
        -Wl,-e,dlmain
)
