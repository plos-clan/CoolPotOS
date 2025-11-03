file(GLOB FATFS_SOURCES "*.c")

add_library(fatfs SHARED ${FATFS_SOURCES})

set_target_properties(fatfs PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/kmod
)

target_include_directories(fatfs PUBLIC
        ${CMAKE_SOURCE_DIR}/module/all_include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(fatfs PRIVATE
        -fPIC
        -fvisibility=hidden
        -fno-stack-protector
        -Wno-incompatible-library-redeclaration
)

target_link_options(fatfs PRIVATE
        -nostdlib
        # 强制覆盖链接器入口点
        -Wl,-e,dlmain
)