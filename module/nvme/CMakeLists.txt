file(GLOB NVME_SOURCES "*.c")

add_library(nvme SHARED ${NVME_SOURCES})

set_target_properties(nvme PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/kmod
)

target_include_directories(nvme PUBLIC
        ${CMAKE_SOURCE_DIR}/module/all_include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_apply_kernel_module_options(nvme)

target_link_options(nvme PRIVATE
        -nostdlib
        # 强制覆盖链接器入口点
        -Wl,-e,dlmain
)
