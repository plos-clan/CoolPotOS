file(GLOB EXTFS_SOURCES
        "*.c"
        "lwext4/src/*.c"
        "lwext4/blockdev/*.c"
)

add_library(extfs SHARED ${EXTFS_SOURCES})

set_target_properties(extfs PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/kmod
)

target_include_directories(extfs PUBLIC
        ${CMAKE_SOURCE_DIR}/module/all_include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/lwext4/include
)

target_compile_options(extfs PRIVATE
        -fPIC
        -fvisibility=hidden
        -fno-stack-protector
)

target_link_options(extfs PRIVATE
        -nostdlib
        # 强制覆盖链接器入口点
        -Wl,-e,dlmain
)