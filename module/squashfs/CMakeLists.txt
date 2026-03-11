set(SQUASHFS_SOURCES
    squashfs.c
    src/data_reader.c
    src/dir_reader.c
    src/frag_table.c
    src/id_table.c
    src/inode.c
    src/meta_reader.c
    src/misc.c
    src/read_inode.c
    src/read_super.c
    src/read_table.c
    src/readdir.c
    src/write_table_stub.c
    src/util/alloc.c
    src/util/array.c
    src/util/rbtree.c
)

add_library(squashfs SHARED ${SQUASHFS_SOURCES})

set_target_properties(squashfs PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/kmod
)

target_include_directories(squashfs PUBLIC
    ${CMAKE_SOURCE_DIR}/module/all_include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(squashfs PRIVATE
    NO_CUSTOM_ALLOC
)

target_compile_options(squashfs PRIVATE
    -fPIC
    -fvisibility=hidden
    -fno-stack-protector
    -Wno-incompatible-library-redeclaration
)

target_link_options(squashfs PRIVATE
    -nostdlib
    -Wl,-Bsymbolic
    -Wl,-e,dlmain
)
