target_compile_options(kernel PRIVATE
        -target loongarch64-freestanding
        -march=loongarch64 -mabi=lp64d
        -mcmodel=medium
        -static
        -nostdinc
        -nostdlib
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-unused-value
        -Wno-incompatible-library-redeclaration
        -Wno-unused-function
        -fstack-protector
        # -fstack-protector-all
        ${COMPILE_MODE}
)

target_link_options(kernel PRIVATE
        -target loongarch64-freestanding
        -T ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/loongarch64/linker.ld
        -static
        -nostdlib
        -fuse-ld=lld
)

file(GLOB_RECURSE LOONGARCH64_SOURCES
        "src/arch/loongarch64/*.c"
        "src/arch/loongarch64/*.S"
)

target_sources(kernel PRIVATE
        ${LOONGARCH64_SOURCES}
        ${FS_SOURCES}
        ${UTIL_SOURCES}
        ${DRIVER_SOURCES}
        ${TERM_SOURCES}
        ${MOD_SOURCES}
        ${MEM_SOURCES}
        ${TASK_SOURCES}
        ${INT_SOURCES}
        ${LIB_SOURCES}
        ${NET_SOURCES}
)

target_include_directories(kernel PUBLIC
        src/arch/loongarch64/include
        src/include/types
        src/include
        ${CMAKE_BINARY_DIR}
)

set_target_properties(kernel PROPERTIES OUTPUT_NAME "cpkrnl_la64.elf")

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(RUN_ARG "")
else ()
    set(RUN_ARG "-s -S")
endif ()

add_custom_target(run
        DEPENDS kernel
        COMMAND echo "qemu-system-loongarch64 -machine virt -cpu la464 -smp 2 -m 2G -kernel $<TARGET_FILE:kernel> -append \"console=ttyS0\" ${RUN_ARG}"
        COMMENT "Running QEMU for LoongArch64..."
)
