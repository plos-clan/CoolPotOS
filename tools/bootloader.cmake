### 引导器构建 (Limine fetch and setup)
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
set(LIMINE_NAME limine)
set(LIMINE_BRANCH "v9.x-binary")

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
