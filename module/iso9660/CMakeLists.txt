file(GLOB ISO9660_SOURCES "*.c")

add_library(iso9660 SHARED ${ISO9660_SOURCES})

set_target_properties(iso9660 PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/kmod
)

target_include_directories(iso9660 PUBLIC
        ${CMAKE_SOURCE_DIR}/module/all_include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_apply_kernel_module_options(iso9660)

target_link_options(iso9660 PRIVATE
        -nostdlib
        # 强制覆盖链接器入口点
        -Wl,-e,dlmain
)
