/*
 * PlantsOS fat FileSystem
 * Copyright by min0911
 */
#include "../include/vdisk.h"
#include "../include/fat.h"
#include "../include/memory.h"
#include "../include/list.h"
#include "../include/common.h"
/*
static inline int get_fat_date(unsigned short year, unsigned short month,
                               unsigned short day) {
    year -= 1980;
    unsigned short date = 0;
    date |= (year & 0x7f) << 9;
    date |= (month & 0x0f) << 5;
    date |= (day & 0x1f);
    return date;
}
static inline int get_fat_time(unsigned short hour, unsigned short minute) {
    unsigned short time = 0;
    time |= (hour & 0x1f) << 11;
    time |= (minute & 0x3f) << 5;
    return time;
}

void read_fat(unsigned char *img, int *fat, unsigned char *ff, int max,
              int type) {
    if (type == 12) {
        for (int i = 0, j = 0; i < max; i += 2) {
            fat[i + 0] = (img[j + 0] | img[j + 1] << 8) & 0xfff;
            fat[i + 1] = (img[j + 1] >> 4 | img[j + 2] << 4) & 0xfff;
            j += 3;
        }
        // 保留簇
        ff[0] = 1;
        ff[1] = 1;
        for (int i = 1; i < max; i++) {
            if (fat[i] > 0 && fat[i] < 0xff0) {
                ff[fat[i]] = 1;
            } else if (fat[i] >= 0xff0 && fat[i] <= 0xfff) {
                ff[i + 1] = 1;
            }
        }
    } else if (type == 16) {
        unsigned short *p = (unsigned short *)img;
        for (int i = 0; i != max; i++) {
            fat[i] = p[i];
        }
        ff[0] = 1;
        ff[1] = 1;
        for (int i = 1; i < max; i++) {
            if (fat[i] > 0 && fat[i] < 0xfff0) {
                ff[fat[i]] = 1;
            } else if (fat[i] >= 0xfff0 && fat[i] <= 0xffff) {
                ff[i + 1] = 1;
            }
        }
    } else if (type == 32) {
        unsigned int *p = (unsigned int *)img;
        for (int i = 0; i != max; i++) {
            fat[i] = p[i];
        }
        ff[0] = 1;
        ff[1] = 1;
        for (int i = 1; i < max; i++) {
            if (fat[i] > 0 && fat[i] < 0xffffff0) {
                ff[fat[i]] = 1;
            } else if (fat[i] >= 0xffffff0 && fat[i] <= 0xfffffff) {
                ff[i + 1] = 1;
            }
        }
    }
    return;
}

int get_directory_max(struct FAT_FILEINFO *directory, vfs_t *vfs) {
    if (directory == get_dm(vfs).root_directory) {
        return get_dm(vfs).RootMaxFiles;
    }
    for (int i = 1; FindForCount(i, get_dm(vfs).directory_list) != NULL; i++) {
        struct List *l = FindForCount(i, get_dm(vfs).directory_list);
        if ((struct FAT_FILEINFO *)l->val == directory) {
            return (int)FindForCount(i, get_dm(vfs).directory_max_list)->val;
        }
    }
}
void file_loadfile(int clustno, int size, char *buf, int *fat, vfs_t *vfs) {
    if (!size) {
        return;
    }
    void *img = kmalloc(((size - 1) / get_dm(vfs).ClustnoBytes + 1) *
                            get_dm(vfs).ClustnoBytes);
    for (int i = 0; i != (size - 1) / get_dm(vfs).ClustnoBytes + 1; i++) {
        uint32_t sec = (get_dm(vfs).FileDataAddress +
                        (clustno - 2) * get_dm(vfs).ClustnoBytes) /
                       get_dm(vfs).SectorBytes;
        Disk_Read(sec, get_dm(vfs).ClustnoBytes / get_dm(vfs).SectorBytes,
                  img + i * get_dm(vfs).ClustnoBytes, vfs->disk_number);
        clustno = fat[clustno];
    }
    memcpy((void *)buf, img, size);
    kfree(img, ((size - 1) / get_dm(vfs).SectorBytes + 1) *
                   get_dm(vfs).SectorBytes);
    return;
}

void file_savefile(int clustno, int size, char *buf, int *fat,
                   unsigned char *ff, vfs_t *vfs) {
    uint32_t clustall = 0;
    int tmp = clustno;
    int end = clustno_end(get_dm(vfs).type);
    while (fat[clustno] != end) { // 计算文件占多少Fat项 Fat项 = 大小 / 簇大小 + 1
        clustno = fat[clustno];
        clustall++;
    }
    int old_clustno = clustno;
    clustno = tmp;
    int alloc_size;
    if (size >
        (clustall + 1) *
        get_dm(vfs).ClustnoBytes) { // 新大小 > (旧大小 / 簇大小 + 1) * 簇大小
        // 请求内存大小 = (新大小 / 簇大小 + 1) * 簇大小
        alloc_size =
                ((size - 1) / get_dm(vfs).ClustnoBytes + 1) * get_dm(vfs).ClustnoBytes;
        // 分配Fat（这里需要在写盘前分配）
        for (int size1 = size; size1 > ((clustall + 1) * get_dm(vfs).ClustnoBytes);
             size1 -= get_dm(vfs).ClustnoBytes) {
            for (int i = 0; i != get_dm(vfs).FatMaxTerms; i++) {
                if (!ff[i]) {
                    fat[old_clustno] = i;
                    old_clustno = i;
                    ff[i] = true;
                    break;
                }
            }
        }
        fat[old_clustno] = end; // 结尾Fat
        ff[old_clustno] = true;
    } else if (size <=
               (clustall + 1) *
               get_dm(vfs).ClustnoBytes) { // 新大小 <= (旧大小 / 簇大小
        // + 1) * 簇大小
        // 请求内存大小 = (旧大小 / 簇大小 + 1) * 簇大小
        alloc_size = (clustall + 1) * get_dm(vfs).ClustnoBytes;
        // 这里不分配Fat的原因是要清空更改后多余的数据
    }
    void *img = kmalloc(alloc_size);
    clean((char *)img, alloc_size);
    memcpy(img, buf, size); // 把要写入的数据复制到新请求的内存地址
    for (int i = 0; i != (alloc_size / get_dm(vfs).ClustnoBytes); i++) {
        uint32_t sec = (get_dm(vfs).FileDataAddress +
                        (clustno - 2) * get_dm(vfs).ClustnoBytes) /
                       get_dm(vfs).SectorBytes;
        Disk_Write(sec, get_dm(vfs).ClustnoBytes / get_dm(vfs).SectorBytes,
                   img + i * get_dm(vfs).ClustnoBytes, vfs->disk_number);
        clustno = fat[clustno];
    }
    kfree(img, alloc_size);
    if (size <
        clustall *
        get_dm(vfs).ClustnoBytes) { // 新大小 < (旧大小 / 簇大小) * 簇大小
        // 分配Fat（中间情况没必要分配）
        int i = old_clustno;
        for (int size1 = clustall * get_dm(vfs).ClustnoBytes; size1 > size;
             size1 -= get_dm(vfs).ClustnoBytes) {
            fat[i] = 0;
            ff[i] = 0;
            for (int j = 0; j != get_dm(vfs).FatMaxTerms; j++) {
                if (fat[j] == i) {
                    i = j;
                }
            }
        }
        old_clustno = i;
        fat[old_clustno] = end;
        ff[old_clustno] = 1;
    }
    file_savefat(fat, 0, get_dm(vfs).FatMaxTerms, vfs);
}

void file_savefat(int *fat, int clustno, int length, vfs_t *vfs) {
    unsigned char *img = get_dm(vfs).ADR_DISKIMG + get_dm(vfs).Fat1Address;
    int size, sec;
    if (get_dm(vfs).type == 12) {
        for (int i = 0; i <= length; i++) {
            if ((clustno + i) % 2 == 0) {
                img[(clustno + i) * 3 / 2 + 0] = fat[clustno + i] & 0xff;
                img[(clustno + i) * 3 / 2 + 1] =
                        (fat[clustno + i] >> 8 | (img[(clustno + i) * 3 / 2 + 1] & 0xf0)) &
                        0xff;
            } else if ((clustno + i) % 2 != 0) {
                img[(clustno + i - 1) * 3 / 2 + 1] =
                        ((img[(clustno + i - 1) * 3 / 2 + 1] & 0x0f) | fat[clustno + i]
                                << 4) &
                        0xff;
                img[(clustno + i - 1) * 3 / 2 + 2] = (fat[clustno + i] >> 4) & 0xff;
            }
        }
        size = length * 3 / 2 - 1;
        sec = clustno * 3 / 2;
    } else if (get_dm(vfs).type == 16) {
        for (int i = 0; i <= length; i++) {
            img[(clustno + i) * 2 + 0] = fat[clustno + i] & 0xff;
            img[(clustno + i) * 2 + 1] = (fat[clustno + i] >> 8) & 0xff;
        }
        size = length * 2 - 1;
        sec = clustno * 2;
    } else if (get_dm(vfs).type == 32) {
        for (int i = 0; i <= length; i++) {
            img[(clustno + i) * 4 + 0] = fat[clustno + i] & 0xff;
            img[(clustno + i) * 4 + 1] = (fat[clustno + i] >> 8) & 0xff;
            img[(clustno + i) * 4 + 2] = (fat[clustno + i] >> 16) & 0xff;
            img[(clustno + i) * 4 + 3] = fat[clustno + i] >> 24;
        }
        size = length * 4 - 1;
        sec = clustno * 4;
    }
    Disk_Write((get_dm(vfs).Fat1Address + sec) / get_dm(vfs).SectorBytes,
               size / get_dm(vfs).SectorBytes + 1,
               get_dm(vfs).ADR_DISKIMG + get_dm(vfs).Fat1Address,
               vfs->disk_number);
    Disk_Write((get_dm(vfs).Fat2Address + sec) / get_dm(vfs).SectorBytes,
               size / get_dm(vfs).SectorBytes + 1,
               get_dm(vfs).ADR_DISKIMG + get_dm(vfs).Fat2Address,
               vfs->disk_number);
}

struct FAT_FILEINFO *file_search(char *name, struct FAT_FILEINFO *finfo,
                                 int max) {
    int i, j;
    char s[12];
    for (j = 0; j < 11; j++) {
        s[j] = ' ';
    }
    j = 0;
    for (i = 0; name[i] != 0; i++) {
        if (j >= 11) {
            return 0; //没有找到
        }
        if (name[i] == '.' && j <= 8) {
            j = 8;
        } else {
            s[j] = name[i];
            if ('a' <= s[j] && s[j] <= 'z') {
                //将小写字母转换为大写字母
                s[j] -= 0x20;
            }
            j++;
        }
    }
    for (i = 0; i < max;) {
        if (finfo[i].name[0] == 0x00) {
            break;
        }
        if ((finfo[i].type & 0x18) == 0) {
            for (j = 0; j < 11; j++) {
                if (finfo[i].name[j] != s[j]) {
                    goto next;
                }
            }
            return finfo + i; //找到文件
        }
        next:
        i++;
    }
    return 0; //没有找到
}
struct FAT_FILEINFO *dict_search(char *name, struct FAT_FILEINFO *finfo,
                                 int max) {
    int i, j;
    char s[12];
    for (j = 0; j < 11; j++) {
        s[j] = ' ';
    }
    j = 0;
    for (i = 0; name[i] != 0; i++) {
        if (j >= 11) {
            return 0; //没有找到
        } else {
            s[j] = name[i];
            if ('a' <= s[j] && s[j] <= 'z') {
                //将小写字母转换为大写字母
                s[j] -= 0x20;
            }
            j++;
        }
    }
    for (i = 0; i < max;) {
        if (finfo[i].name[0] == 0x00) {
            break;
        }
        if (finfo[i].type == 0x10) {
            for (j = 0; j < 11; j++) {
                if (finfo[i].name[j] != s[j]) {
                    goto next;
                }
            }
            return finfo + i; // 找到文件
        }
        next:
        i++;
    }
    return 0; //没有找到
}
struct FAT_FILEINFO *Get_File_Address(char *path1, vfs_t *vfs) {
    // TODO: Modifly it
    struct FAT_FILEINFO *bmpDict = get_now_dir(vfs);
    int drive_number = vfs->disk_number;
    char *path = (char *)page_malloc(strlen(path1) + 1);
    char *bmp = path;
    strcpy(path, path1);
    strtoupper(path);
    if (strncmp("/", path, 1) == 0) {
        path += 1;
        bmpDict = get_dm(vfs).root_directory;
    }
    if (path[0] == '\\' || path[0] == '/') {
        //跳过反斜杠和正斜杠
        for (int i = 0; i < strlen(path); i++) {
            if (path[i] != '\\' && path[i] != '/') {
                path += i;
                break;
            }
        }
    }
    char *temp_name = (char *)kmalloc(128);
    struct FAT_FILEINFO *finfo = get_dm(vfs).root_directory;
    int i = 0;
    while (1) {
        int j;
        for (j = 0; i < strlen(path); i++, j++) {
            if (path[i] == '\\' || path[i] == '/') {
                temp_name[j] = '\0';
                i++;
                break;
            }
            temp_name[j] = path[i];
        }
        finfo = dict_search(temp_name, bmpDict, get_directory_max(bmpDict, vfs));
        if (finfo == 0) {
            if (path[i] != '\0') {
                kfree((void *)temp_name, 128);
                kfree((void *)bmp, strlen(path1) + 1);
                return 0;
            }
            finfo = file_search(temp_name, bmpDict, get_directory_max(bmpDict, vfs));
            if (finfo == 0) {
                kfree((void *)temp_name, 128);
                kfree((void *)bmp, strlen(path1) + 1);
                return 0;
            } else {
                goto END;
            }
        } else {
            if (get_clustno(finfo->clustno_high, finfo->clustno_low) != 0) {
                for (int count = 1;
                     FindForCount(count, get_dm(vfs).directory_clustno_list) != NULL;
                     count++) {
                    struct List *list =
                            FindForCount(count, get_dm(vfs).directory_clustno_list);
                    if (get_clustno(finfo->clustno_high, finfo->clustno_low) ==
                                                                             list->val) {
                        list = FindForCount(count, get_dm(vfs).directory_list);
                        bmpDict = (struct FAT_FILEINFO *)list->val;
                        break;
                    }
                }
            } else {
                bmpDict = get_dm(vfs).root_directory;
            }
            clean(temp_name, 128);
        }
    }
    END:
    kfree((void *)temp_name, 128);
    kfree((void *)bmp, strlen(path1) + 1);
    return finfo;
}
*/