target_compile_options(kernel PRIVATE
        -target x86_64-freestanding
        -mno-80387 -mno-mmx -mno-sse -mno-sse2
        -mno-red-zone -msoft-float
        -fPIC
        -nostdinc
        -nostdlib
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-unused-value
        -Wno-incompatible-library-redeclaration
        -Wno-unused-function
        -flto
        # -fstack-protector
        # -fstack-protector-all
        ${COMPILE_MODE}
)

target_link_options(kernel PRIVATE
        -target x86_64-freestanding
        -T ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86_64/linker.ld
        -nostdlib
        -fno-stack-protector
        -fuse-ld=lld
)

file(GLOB_RECURSE X86_64_SOURCES
        "src/arch/x86_64/*.c"
        "src/arch/x86_64/*.S"
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
        ${LIB_SOURCES}
)

target_include_directories(kernel PUBLIC
        src/arch/x86_64/include
        src/include/types
        src/include
        ${CMAKE_BINARY_DIR}
)

target_link_libraries(kernel PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/libllvm_x64.a
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/liballoc-x86_64.a
)

set_target_properties(kernel PROPERTIES OUTPUT_NAME "cpkrnl_x64.elf")

# === ISO 构建 ===
set(LIMINE_SHARE_DIR "${LIMINE_TMP_DIR}/share/limine")
set(ISO_FILE ${CMAKE_CURRENT_BINARY_DIR}/CoolPotOS.iso)
set(ISO_DIR ${CMAKE_CURRENT_BINARY_DIR}/iso_dir)
set(ROOTFS_IMAGE ${CMAKE_CURRENT_SOURCE_DIR}/assets/cp_rootfs.sfs)
set(INITRAMFS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/initramfs)
set(INITRAMFS_IMAGE ${CMAKE_CURRENT_BINARY_DIR}/initramfs.img)
set(INITRAMFS_SOURCE_DEPS
        ${INITRAMFS_DIR}/init
        ${INITRAMFS_DIR}/bin/busybox
        ${INITRAMFS_DIR}/bin/zstd
        ${INITRAMFS_DIR}/etc/passwd
        ${INITRAMFS_DIR}/etc/securetty
        ${INITRAMFS_DIR}/etc/shadow
)

add_custom_command(
        OUTPUT ${INITRAMFS_IMAGE}
        COMMAND ${CMAKE_COMMAND} -E env PATH=/usr/bin:/bin
                ${CMAKE_COMMAND} -E chdir ${INITRAMFS_DIR} /bin/sh -c "/usr/bin/find . -print | /usr/bin/sort | /bin/cpio -o -H newc > '${INITRAMFS_IMAGE}'"
        DEPENDS ${INITRAMFS_SOURCE_DEPS}
        COMMENT "Packing initramfs image"
        VERBATIM
)

add_custom_target(initramfs_image DEPENDS ${INITRAMFS_IMAGE})

add_custom_target(iso ALL
        DEPENDS kernel e1000 fatfs iso9660 zstd squashfs fetch_limine_binaries initramfs_image

        COMMAND ${CMAKE_COMMAND} -E remove_directory ${ISO_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ISO_DIR}/limine
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/assets/readme.txt ${ISO_DIR}/readme.txt
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${INITRAMFS_IMAGE} ${ISO_DIR}/initramfs.img
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/assets/limine.conf ${ISO_DIR}/limine.conf
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/assets/background.jpg ${ISO_DIR}/background.jpg
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ROOTFS_IMAGE} ${ISO_DIR}/cp_rootfs.sfs

        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:kernel> ${ISO_DIR}/cpkrnl_x64.elf

        COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/sign_module.py $<TARGET_FILE:fatfs> ${CMAKE_BINARY_DIR}/keys/module_signing_priv.pem
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/sign_module.py $<TARGET_FILE:e1000> ${CMAKE_BINARY_DIR}/keys/module_signing_priv.pem
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/sign_module.py $<TARGET_FILE:iso9660> ${CMAKE_BINARY_DIR}/keys/module_signing_priv.pem
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/sign_module.py $<TARGET_FILE:zstd> ${CMAKE_BINARY_DIR}/keys/module_signing_priv.pem
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/sign_module.py $<TARGET_FILE:squashfs> ${CMAKE_BINARY_DIR}/keys/module_signing_priv.pem

        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:fatfs> ${ISO_DIR}/fatfs.km
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:e1000> ${ISO_DIR}/e1000.km
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:iso9660> ${ISO_DIR}/iso9660.km
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:zstd> ${ISO_DIR}/zstd.km
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:squashfs> ${ISO_DIR}/squashfs.km

        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIMINE_SHARE_DIR}/limine-bios.sys ${ISO_DIR}/limine/limine-bios.sys
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIMINE_SHARE_DIR}/limine-bios-cd.bin ${ISO_DIR}/limine/limine-bios-cd.bin
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LIMINE_SHARE_DIR}/limine-uefi-cd.bin ${ISO_DIR}/limine/limine-uefi-cd.bin

        COMMAND xorriso -as mkisofs -R -r -J -b limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus -apm-block-size 2048 --efi-boot limine/limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label ${ISO_DIR} -o ${ISO_FILE}

        COMMENT "ISO image created at: ${ISO_FILE}"
)

# === Run target ===
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(RUN_ARG)
else ()
    set(RUN_ARG "-s -S")
endif ()

add_custom_target(run
        DEPENDS iso
        COMMAND echo "qemu-system-x86_64 -M q35 -cpu Haswell,+x2apic,+avx -smp 1 -serial stdio -m 2048M -audiodev sdl,id=audio0 -device sb16,audiodev=audio0 -netdev user,id=net0 -device e1000,netdev=net0 -rtc base=utc -vga vmware -drive if=pflash,format=raw,file=${CMAKE_CURRENT_SOURCE_DIR}/assets/ovmf-code_x64.fd -drive if=none,file=${PROJECT_ROOT_DIR}/rootfs-x86_64.img,format=raw,id=harddisk -device nvme,drive=harddisk,serial=1234 -cdrom ${ISO_FILE} ${RUN_ARG}"
        COMMENT "Running QEMU for x86_64 ISO..."
)
