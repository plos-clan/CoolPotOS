#include "cpio.h"
#include "cpfs.h"
#include "errno.h"
#include "iso9660.h"
#include "kprint.h"
#include "krlibc.h"
#include "module.h"
#include "sprintf.h"
#include "vfs.h"

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

    struct cpio_newc_header_t hdr;
    size_t                    offset = 0;
    while (true) {
        memcpy(&hdr, init_ramfs->data + offset, sizeof(hdr));
        offset += sizeof(hdr);

        size_t namesize = read_num(hdr.c_namesize, 8);
        char   filename[namesize + 1];
        filename[0] = '/';
        memcpy(filename + 1, init_ramfs->data + offset, namesize);
        offset = (offset + namesize + 3) & ~3;

        size_t         filesize = read_num(hdr.c_filesize, 8);
        unsigned char *filedata = malloc(filesize);
        memcpy(filedata, init_ramfs->data + offset, filesize);
        offset = (offset + filesize + 3) & ~3;

        /*
        logkf("%.*s\n", (int)sizeof(hdr.c_magic), hdr.c_magic);
        logkf("%.*s\n", (int)sizeof(hdr.c_mode), hdr.c_mode);
        logkf("%.*s\n", (int)(namesize - 1), filename);
        logkf("%.*s\n", (int)filesize, filedata);
        */

        if (!strcmp(filename, "/TRAILER!!!")) {
            free(filedata);
            break;
        }
        if (strcmp(filename, "/.") == 0) {
            free(filedata);
            continue;
        }

        if (read_num(hdr.c_mode, 8) & 040000) {
            errno_t status = vfs_mkdir(filename);
            if (status != EOK) {
                kerror("Cannot build initramfs directory(%s), error code: %d", filename, status);
                return;
            }
        } else {
            errno_t status = vfs_mkfile(filename);
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
}
