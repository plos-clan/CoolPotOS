#include "cpio.h"
#include "cpfs.h"
#include "errno.h"
#include "kprint.h"
#include "krlibc.h"
#include "module.h"
#include "sprintf.h"
#include "vfs.h"
#include "zstd/zstd.h"

static char *get_pdir_fpath(char *path) {
    if (path == NULL) { return NULL; }

    if (strcmp(path, "/") == 0) { return strdup("/"); }

    size_t len        = strlen(path);
    char  *last_slash = strrchr(path, '/');

    if (last_slash == NULL) { return strdup("."); }

    if (last_slash == path) { return strdup("/"); }

    size_t new_len = last_slash - path;

    if (path[len - 1] == '/') {
        char *prev_last_slash = (char *)last_slash - 1;
        while (prev_last_slash > path && *prev_last_slash == '/') {
            prev_last_slash--;
        }
        while (prev_last_slash > path && *prev_last_slash != '/') {
            prev_last_slash--;
        }

        if (*prev_last_slash == '/') {
            new_len = prev_last_slash - path;
        } else {
            new_len = 0;
        }
    }

    char *new_path = (char *)malloc(new_len + 1);
    if (new_path == NULL) { return NULL; }

    strncpy(new_path, path, new_len);
    new_path[new_len] = '\0';
    if (new_len == 0 && path[0] == '/') {
        free(new_path);
        return strdup("/");
    }

    return new_path;
}

compression_type_t get_compression_type(const void *data, size_t size) {
    if (size < 4) {
        return COMPRESSION_UNKNOWN; // 数据太小，无法判断
    }
    const unsigned char *bytes = (const unsigned char *)data;
    if (bytes[0] == 0x1F && bytes[1] == 0x8B) { return COMPRESSION_GZIP; }

    if (size >= 6 && bytes[0] == 0xFD && bytes[1] == 0x37 && bytes[2] == 0x7A && bytes[3] == 0x58 &&
        bytes[4] == 0x5A && bytes[5] == 0x00) {
        return COMPRESSION_XZ;
    }

    if (bytes[0] == 0x18 && bytes[1] == 0x4D && bytes[2] == 0x22 && bytes[3] == 0x04) {
        return COMPRESSION_LZ4;
    }

    if (bytes[0] == 0x28 && bytes[1] == 0xB5 && bytes[2] == 0x2F && bytes[3] == 0xFD) {
        return COMPRESSION_ZSTD;
    }

    if (bytes[0] == 0x5D && bytes[1] == 0x00 && bytes[2] == 0x00 && bytes[3] == 0x80) {
        return COMPRESSION_LZMA;
    }

    if (strncmp((const char *)bytes, "070701", 6) == 0 ||
        strncmp((const char *)bytes, "070707", 6) == 0) {
        return COMPRESSION_NONE;
    }

    return COMPRESSION_UNKNOWN;
}

static size_t read_num(const char *str, size_t count) {
    size_t val = 0;
    for (size_t i = 0; i < count; ++i)
        val = val * 16 + (isdigit(str[i]) ? str[i] - '0' : str[i] - 'A' + 10);
    return val;
}

void cpio_init(void) {
    cp_module_t *init_ramfs = get_module("initramfs");
    if (!init_ramfs) return;
    cpfs_setup();
    vfs_node_t root = get_rootdir();
    if (vfs_mount((const char *)CPFS_REGISTER_ID, root) != VFS_STATUS_SUCCESS) {
        kerror("Cannot mount cpfs to root_dir");
        return;
    }

    compression_type_t type    = get_compression_type(init_ramfs->data, init_ramfs->size);
    uint8_t           *data_d  = NULL;
    size_t             size_d  = 0;
    bool               is_free = false;

    char *compress_type;
    switch (type) {
    case COMPRESSION_NONE:
        data_d        = init_ramfs->data;
        size_d        = init_ramfs->size;
        is_free       = false;
        compress_type = "cpio";
        break;
    case COMPRESSION_ZSTD:
        size_t dict_size = ZSTD_getFrameContentSize(init_ramfs->data, init_ramfs->size);
        void  *dict_data = malloc(dict_size);
        ZSTD_decompress(dict_data, dict_size, init_ramfs->data, init_ramfs->size);
        data_d        = dict_data;
        size_d        = dict_size;
        is_free       = true;
        compress_type = "zstd";
        break;
    default: kerror("Cannot load initramfs, unknown format."); return;
    }

next:
    struct cpio_newc_header_t hdr;
    size_t                    offset       = 0;
    size_t                    file_num_all = 0;
    while (true) {
        memcpy(&hdr, data_d + offset, sizeof(hdr));
        offset += sizeof(hdr);

        size_t namesize = read_num(hdr.c_namesize, 8);
        char   filename[namesize + 1];
        filename[0] = '/';
        memcpy(filename + 1, data_d + offset, namesize);
        offset = (offset + namesize + 3) & ~3;

        size_t         filesize = read_num(hdr.c_filesize, 8);
        unsigned char *filedata = malloc(filesize);
        memcpy(filedata, data_d + offset, filesize);
        offset = (offset + filesize + 3) & ~3;

        if (!strcmp(filename, "/TRAILER!!!")) {
            free(filedata);
            break;
        }
        if (strcmp(filename, "/.") == 0) {
            free(filedata);
            continue;
        }

        file_num_all++;
        size_t  mode = read_num(hdr.c_mode, 8);
        errno_t status;
        if (mode & 040000) {
            status = vfs_mkdir(filename);
            if (status != EOK) {
                kerror("Cannot build initramfs directory(%s), error code: %d", filename, status);
                return;
            }
        } else if ((mode & 0120000) == 0120000) {
            const size_t len          = strlen(filename) + filesize;
            char        *all_path     = malloc(len);
            char        *dirname      = get_pdir_fpath(filename);
            char        *symlink_path = calloc(1, filesize + 1);
            strncpy(symlink_path, filedata, filesize);
            sprintf(all_path, "%s/%s", dirname, symlink_path);
            char *target_name = normalize_path(all_path);
            status            = vfs_symlink(filename, target_name);
            free(all_path);
            free(target_name);
            free(dirname);
            free(symlink_path);
            if (status != EOK) {
                kerror("Cannot build initramfs symlink(%s), error code: %d", filename, status);
                return;
            }
        } else {
            status = vfs_mkfile(filename);
            if (status != EOK) {
                kerror("Cannot build initramfs file(%s), error code: %d", filename, status);
                return;
            }
            vfs_node_t file = vfs_open(filename);
            if (file == NULL) {
                kerror("Cannot build initramfs, open error(%s)", filename);
                return;
            }
            status = vfs_write(file, filedata, 0, filesize);
            if (status == VFS_STATUS_FAILED) {
                kerror("Cannot build initramfs, write error(%s)", filename);
                return;
            }
            vfs_close(file);
        }
        free(filedata);
    }
    if (is_free) free(data_d);

    kinfo("Loaded initramfs size:%llu files:%llu compress: %s", init_ramfs->size, file_num_all,
          compress_type);
}
