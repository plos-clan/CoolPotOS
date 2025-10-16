cmake_minimum_required(VERSION 3.16)
project(CoolPotOS LANGUAGES C ASM)

include(FetchContent)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_ASM_COMPILER clang)
set(CMAKE_LINKER lld)
set(LIMINE_TMP_DIR ${CMAKE_CURRENT_BINARY_DIR}/limine_temp)

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
set(LIMINE_EXE_NAME "")

if (HOST_PROCESSOR STREQUAL "x86_64" OR HOST_PROCESSOR STREQUAL "amd64")
    set(LIMINE_EXE_NAME "limine-x86_64")
elseif (HOST_PROCESSOR STREQUAL "i686" OR HOST_PROCESSOR STREQUAL "i386")
    set(LIMINE_EXE_NAME "limine-i686")
elseif (HOST_PROCESSOR STREQUAL "aarch64" OR HOST_PROCESSOR STREQUAL "arm64")
    set(LIMINE_EXE_NAME "limine-aarch64")
else ()
    message(FATAL_ERROR "Unsupported host processor for Limine installer: ${HOST_PROCESSOR}. Please check Limine's v9.x-binary branch for compatible executables.")
endif ()


set(LIMINE_REPO_URL "https://github.com/limine-bootloader/limine.git")
set(LIMINE_BRANCH "v9.x-binary")
set(LIMINE_NAME limine_v9_binary)

FetchContent_Declare(
        ${LIMINE_NAME}
        GIT_REPOSITORY ${LIMINE_REPO_URL}
        GIT_TAG ${LIMINE_BRANCH}
        SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/_deps/${LIMINE_NAME}-src
)

FetchContent_MakeAvailable(${LIMINE_NAME})
FetchContent_GetProperties(${LIMINE_NAME}
        SOURCE_DIR LIMINE_CLONE_DIR
)

add_custom_target(fetch_limine_binaries
        DEPENDS ${LIMINE_NAME}

        COMMAND ${CMAKE_COMMAND} -E make_directory ${LIMINE_TMP_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${LIMINE_TMP_DIR}/share/limine

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${LIMINE_CLONE_DIR}/${LIMINE_EXE_NAME}
        ${LIMINE_TMP_DIR}/limine

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${LIMINE_CLONE_DIR}/limine-bios.sys
        ${LIMINE_TMP_DIR}/share/limine/limine-bios.sys

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${LIMINE_CLONE_DIR}/limine-bios-cd.bin
        ${LIMINE_TMP_DIR}/share/limine/limine-bios-cd.bin

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${LIMINE_CLONE_DIR}/limine-uefi-cd.bin
        ${LIMINE_TMP_DIR}/share/limine/limine-uefi-cd.bin

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${LIMINE_CLONE_DIR}/BOOTX64.EFI
        ${LIMINE_TMP_DIR}/share/limine/BOOTX64.EFI

        COMMAND ${CMAKE_COMMAND} -E chmod ${LIMINE_TMP_DIR}/limine +x
        COMMENT "Limine binaries fetched and copied to ${LIMINE_TMP_DIR}"
)
###

add_executable(kernel)

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

target_sources(kernel PRIVATE
        src/fs/**.c
        src/term/**.c
        src/util/**.c
        src/driver/**.c
        src/mod/**.c
)

target_include_directories(kernel PUBLIC
        src/include/types
        src/include
)

if (TARGET_ARCH STREQUAL "x86_64")
    # 1. 编译器标志
    target_compile_options(kernel PRIVATE
            # Freestanding Target
            -target x86_64-freestanding
            # CPU 特性禁用
            -mno-80387 -mno-mmx -mno-sse -mno-sse2
            # ABI/栈设置
            -mno-red-zone -msoft-float
    )

    # 2. 链接器标志
    target_link_options(kernel PRIVATE
            -target x86_64-freestanding
            -T ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/linker.ld
            -nostdlib
            -fuse-ld=lld
    )

    # 3. 架构文件和头文件
    target_sources(kernel PRIVATE src/arch/x86_64/**.c)
    target_include_directories(kernel PUBLIC
            src/arch/x86_64/include
    )

    set_target_properties(kernel PROPERTIES OUTPUT_NAME "cpkrnl_x64.elf")

elseif (TARGET_ARCH STREQUAL "riscv64")
    target_compile_options(kernel PRIVATE
            -target riscv64-unknown-elf
            -march=rv64gc -mabi=lp64d -mcmodel=medany -mno-relax
    )
    target_link_options(kernel PRIVATE
            -target riscv64-unknown-elf
            -T ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/riscv64/linker.ld
            -nostdlib
            -fuse-ld=lld
    )
    target_sources(kernel PRIVATE src/arch/riscv64/**.c)
    target_include_directories(kernel PUBLIC
            src/arch/riscv64/include
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


if(TARGET_ARCH STREQUAL "x86_64")
    add_custom_target(iso ALL
            # 依赖于 kernel 和所有模块目标
            DEPENDS kernel extfs e1000 nvme hid xhci

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

            set(LIMINE_SHARE_DIR "share/limine")

            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIMINE_SHARE_DIR}/limine-bios.sys ${ISO_DIR}/limine/limine-bios.sys
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIMINE_SHARE_DIR}/limine-bios-cd.bin ${ISO_DIR}/limine/limine-bios-cd.bin
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIMINE_SHARE_DIR}/limine-uefi-cd.bin ${ISO_DIR}/limine/limine-uefi-cd.bin

            COMMAND xorriso -as mkisofs -R -r -J -b limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus -apm-block-size 2048 --efi-boot limine/limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label ${ISO_DIR} -o ${ISO_FILE}

            COMMAND limine bios-install ${ISO_FILE}
            COMMENT "ISO image created at: ${ISO_FILE}"
    )

    add_custom_target(run
            DEPENDS iso
            COMMAND qemu-system-x86_64 -M q35 -cpu Haswell,+x2apic,+avx -smp 4 -serial stdio -m 2048M -audiodev sdl,id=audio0 -device sb16,audiodev=audio0 -netdev user,id=net0 -device e1000,netdev=net0 -drive if=pflash,format=raw,file=assets/ovmf-code.fd -cdrom ${ISO_FILE}
            COMMENT "Running QEMU for x86_64 ISO..."
    )

endif()
