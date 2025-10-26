file(GLOB LWEXT_SOURCES "*.c")

add_library(lwext SHARED ${LWEXT_SOURCES})

set_target_properties(lwext PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/kmod
)

target_include_directories(lwext PUBLIC
        ${CMAKE_SOURCE_DIR}/module/all_include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(lwext PRIVATE
        -fPIC
        -fvisibility=hidden
        -fno-stack-protector
)

target_link_options(lwext PRIVATE
        -nostdlib
        # 强制覆盖链接器入口点
        -Wl,-e,dlmain
)