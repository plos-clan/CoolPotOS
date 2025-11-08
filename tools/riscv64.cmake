
target_compile_options(kernel PRIVATE
        -target riscv64-unknown-elf
        -march=rv64gc -mabi=lp64d -mcmodel=medany -mno-relax
        ${COMPILE_MODE}
)

target_link_options(kernel PRIVATE
        -target riscv64-unknown-elf
        -T ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/riscv64/linker.ld
        -nostdlib
        -fuse-ld=lld
)

file(GLOB_RECURSE RISCV64_SOURCES
        "src/arch/riscv64/*.c"
)

target_sources(kernel PRIVATE
        ${RISCV64_SOURCES}
        ${FS_SOURCES}
        ${UTIL_SOURCES}
        ${DRIVER_SOURCES}
        ${TERM_SOURCES}
        ${MOD_SOURCES}
        ${MEM_SOURCES}
        ${TASK_SOURCES}
        ${INT_SOURCES}
        ${LIB_SOURCES}
)

target_include_directories(kernel PUBLIC
        src/arch/riscv64/include
        src/include/types
        src/include
        ${CMAKE_BINARY_DIR}
)

target_link_libraries(kernel PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libgcc_rv64.a
)

set_target_properties(kernel PROPERTIES OUTPUT_NAME "cpkrnl_rv64.elf")

# === Run target for RISC-V (no ISO needed for now, direct QEMU) ===
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(RUN_ARG)
else()
    set(RUN_ARG -s -S)
endif()

add_custom_target(run
        DEPENDS kernel
        COMMAND echo qemu-system-riscv64 -machine virt -cpu rv64 -smp 2 -m 2G -kernel $<TARGET_FILE:kernel>
        # -initrd ${CMAKE_CURRENT_SOURCE_DIR}/assets/initramfs.img
        -append "console=ttyS0"
        ${RUN_ARG}
        COMMENT "Running QEMU for RISC-V64..."
)