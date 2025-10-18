cmake_minimum_required(VERSION 3.16)
project(CoolPotOS LANGUAGES C ASM)

set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

include(FetchContent)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)
set(CMAKE_LINKER lld)
set(LIMINE_TMP_DIR ${CMAKE_CURRENT_BINARY_DIR}/limine_temp)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose build type: Debug Release RelWithDebInfo MinSizeRel")
endif ()

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(TARGET_ARCH x86_64)
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv64")
    set(TARGET_ARCH riscv64)
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(TARGET_ARCH riscv64)
else ()
    message(WARNING "Unsupported or unspecified architecture. Defaulting to x86_64.")
    set(TARGET_ARCH x86_64)
endif ()

### 引导器构建

set(HOST_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})
set(LIMINE_UEFI_NAME "")

if (HOST_PROCESSOR STREQUAL "x86_64" OR HOST_PROCESSOR STREQUAL "amd64")
    set(LIMINE_UEFI_NAME "BOOTX64.EFI")
elseif (HOST_PROCESSOR STREQUAL "i686" OR HOST_PROCESSOR STREQUAL "i386")
    set(LIMINE_UEFI_NAME "BOOTIA32.EFI")
elseif (HOST_PROCESSOR STREQUAL "aarch64" OR HOST_PROCESSOR STREQUAL "arm64")
    set(LIMINE_UEFI_NAME "BOOTAA64.EFI")
elseif (HOST_PROCESSOR STREQUAL "riscv64" OR HOST_PROCESSOR STREQUAL "arm64")
    set(LIMINE_UEFI_NAME "BOOTRISCV64.EFI")
elseif (HOST_PROCESSOR STREQUAL "loongarch64" OR HOST_PROCESSOR STREQUAL "arm64")
    set(LIMINE_UEFI_NAME "BOOTLOONGARCH64.EFI")
else ()
    message(FATAL_ERROR "Unsupported host processor for Limine installer: ${HOST_PROCESSOR}. Please check Limine's v9.x-binary branch for compatible executables.")
endif ()

set(FETCHCONTENT_QUIET FALSE)
set(LIMINE_REPO_URL "https://codeberg.org/Limine/Limine.git")
set(LIMINE_BRANCH "v9.x-binary")
set(LIMINE_NAME limine_v9_binary)

FetchContent_Declare(
        ${LIMINE_NAME}
        GIT_REPOSITORY ${LIMINE_REPO_URL}
        GIT_TAG ${LIMINE_BRANCH}
        SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps/${LIMINE_NAME}-src
        GIT_PROGRESS 1
)

FetchContent_MakeAvailable(${LIMINE_NAME})
FetchContent_GetProperties(${LIMINE_NAME}
        SOURCE_DIR LIMINE_CLONE_DIR
)

add_custom_target(fetch_limine_binaries
        COMMAND ${CMAKE_COMMAND} -E make_directory ${LIMINE_TMP_DIR}

        COMMAND ${CMAKE_COMMAND} -E make_directory ${LIMINE_TMP_DIR}/share/limine

        COMMAND ${CMAKE_COMMAND} -E make_directory ${LIMINE_TMP_DIR}/limine

        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIMINE_CLONE_DIR}/limine-bios.sys ${LIMINE_TMP_DIR}/share/limine/limine-bios.sys

        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIMINE_CLONE_DIR}/limine-bios-cd.bin ${LIMINE_TMP_DIR}/share/limine/limine-bios-cd.bin

        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIMINE_CLONE_DIR}/limine-uefi-cd.bin ${LIMINE_TMP_DIR}/share/limine/limine-uefi-cd.bin

        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIMINE_CLONE_DIR}/${LIMINE_UEFI_NAME} ${LIMINE_TMP_DIR}/share/limine/${LIMINE_UEFI_NAME}

        COMMENT "Limine binaries fetched and copied to ${LIMINE_TMP_DIR}"
)
###

add_subdirectory(module/e1000)

add_executable(kernel)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(COMPILE_MODE
            -g0
            -O3
    )
else()
    set(COMPILE_MODE
            -g3
            -O0
    )
endif()

target_compile_options(kernel PRIVATE
        -nostdinc
        -nostdlib
        -fPIC
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-unused-value
        -Wno-incompatible-library-redeclaration
        -Wno-unused-function
        -flto
)

file(GLOB_RECURSE X86_64_SOURCES
        "src/arch/x86_64/*.c"
        "src/arch/x86_64/*.S"
)

file(GLOB_RECURSE RISCV64_SOURCES
        "src/arch/riscv64/*.c"
)

file(GLOB_RECURSE FS_SOURCES "src/fs/*.c")
file(GLOB_RECURSE UTIL_SOURCES "src/util/*.c")
file(GLOB_RECURSE TERM_SOURCES "src/term/*.c")
file(GLOB_RECURSE DRIVER_SOURCES "src/driver/*.c")
file(GLOB_RECURSE MOD_SOURCES "src/mod/*.c")
file(GLOB_RECURSE MEM_SOURCES "src/mem/*.c")
file(GLOB_RECURSE TASK_SOURCES "src/task/*.c")
file(GLOB_RECURSE INT_SOURCES "src/int/*.c")

if (TARGET_ARCH STREQUAL "x86_64")

    target_compile_options(kernel PRIVATE
            -target x86_64-freestanding
            -mno-80387 -mno-mmx -mno-sse -mno-sse2
            -mno-red-zone -msoft-float
            ${COMPILE_MODE}
    )

    target_link_options(kernel PRIVATE
            -target x86_64-freestanding
            -T ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/linker.ld
            -nostdlib
            -fno-stack-protector
            -fuse-ld=lld
    )

    target_sources(kernel PRIVATE
            ${X86_64_SOURCES}
            ${FS_SOURCES}
            ${UTIL_SOURCES}
            ${DRIVER_SOURCES}
            ${TERM_SOURCES}
            ${MOD_SOURCES}
            ${MEM_SOURCES}
            ${TASK_SOURCES}
            ${INT_SOURCES}
    )
    target_include_directories(kernel PUBLIC
            src/arch/x86_64/include
            src/include/types
            src/include
    )
    target_link_libraries(kernel PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/libllvm_x64.a
    )

    set_target_properties(kernel PROPERTIES OUTPUT_NAME "cpkrnl_x64.elf")

elseif (TARGET_ARCH STREQUAL "riscv64")
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
    )
    target_include_directories(kernel PUBLIC
            src/arch/riscv64/include
            src/include/types
            src/include
    )
    target_link_libraries(kernel PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/libgcc_rv64.a
    )
    set_target_properties(kernel PROPERTIES OUTPUT_NAME "cpkrnl_rv64.elf")
endif ()

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

if (TARGET_ARCH STREQUAL "x86_64")
    set(LIMINE_SHARE_DIR "${LIMINE_TMP_DIR}/share/limine")
    add_custom_target(iso ALL
            # 依赖于 kernel 和所有模块目标
            DEPENDS kernel e1000 fetch_limine_binaries

            # 步骤 1: 创建 ISO 目录并复制文件 (xmake的 os.cp)
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${ISO_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${ISO_DIR}/limine
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/assets/readme.txt ${ISO_DIR}/readme.txt
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/assets/limine.conf ${ISO_DIR}/limine.conf
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/assets/background.jpg ${ISO_DIR}/background.jpg

            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:kernel> ${ISO_DIR}/cpkrnl_x64.elf

            #COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:extfs> ${ISO_DIR}/extfs.km
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:e1000> ${ISO_DIR}/e1000.km
            #COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:nvme> ${ISO_DIR}/nvme.km
            #COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:hid> ${ISO_DIR}/hid.km
            #COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:xhci> ${ISO_DIR}/xhci.km

            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIMINE_SHARE_DIR}/limine-bios.sys ${ISO_DIR}/limine/limine-bios.sys
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIMINE_SHARE_DIR}/limine-bios-cd.bin ${ISO_DIR}/limine/limine-bios-cd.bin
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIMINE_SHARE_DIR}/limine-uefi-cd.bin ${ISO_DIR}/limine/limine-uefi-cd.bin

            COMMAND xorriso -as mkisofs -R -r -J -b limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus -apm-block-size 2048 --efi-boot limine/limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label ${ISO_DIR} -o ${ISO_FILE}

            COMMENT "ISO image created at: ${ISO_FILE}"
    )

    if (CMAKE_BUILD_TYPE STREQUAL "Release")
        set(RUN_ARG)
    else()
        set(RUN_ARG
                -s
                -S
        )
    endif()

    add_custom_target(run
            DEPENDS iso
            COMMAND qemu-system-x86_64 -M q35 -cpu Haswell,+x2apic,+avx -smp 4 -serial stdio -m 2048M -audiodev sdl,id=audio0 -device sb16,audiodev=audio0 -netdev user,id=net0 -device e1000,netdev=net0 -drive if=pflash,format=raw,file=${CMAKE_CURRENT_SOURCE_DIR}/assets/ovmf-code_x64.fd -cdrom ${ISO_FILE} ${RUN_ARG}
            COMMENT "Running QEMU for x86_64 ISO..."
    )

endif ()
