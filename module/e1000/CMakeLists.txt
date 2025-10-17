file(GLOB E1000_SOURCES "*.c")

add_library(e1000 SHARED ${E1000_SOURCES})

set_target_properties(e1000 PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/kmod
)

target_include_directories(e1000 PUBLIC
        ${CMAKE_SOURCE_DIR}/module/all_include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(e1000 PRIVATE
        -fPIC
        -fvisibility=hidden
        -fno-stack-protector
)

target_link_options(e1000 PRIVATE
        -nostdlib
        # 强制覆盖链接器入口点
        -Wl,-e,dlmain
)