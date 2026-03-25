#include "driver/drm/drm.h"
#include "driver/drm/drm_core.h"
#include "driver/drm/drm_device.h"
#include "driver/drm/drm_fourcc.h"
#include "driver/pci/pci.h"
#include "errno.h"
#include "fs/sysfs.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "task/poll.h"
#include "task/scheduler.h"
#include "term/klog.h"
#include "timer.h"

#define HZ 60
#define DRM_MAX_USER_BLOBS    256
#define DRM_USER_BLOB_MAX_SIZE (64 * 1024)
#define DRM_BLOB_ID_CRTC_MODE_BASE      0x10000000U
#define DRM_BLOB_ID_CONNECTOR_EDID_BASE 0x20000000U
#define DRM_BLOB_ID_PLANE_IN_FORMATS_BASE 0x28000000U
#define DRM_BLOB_ID_USER_BASE           0x30000000U
#define DRM_BLOB_ID_USER_LAST           0x3fffffffU

static int drm_id = 0;
static uint32_t drm_user_blob_next_id = DRM_BLOB_ID_USER_BASE + 1;
static spin_t drm_user_blobs_lock     = SPIN_INIT;

typedef struct drm_user_blob_entry {
    bool used;
    drm_device_t *dev;
    uint32_t blob_id;
    uint32_t length;
    void *data;
} drm_user_blob_entry_t;

static drm_user_blob_entry_t drm_user_blobs[DRM_MAX_USER_BLOBS];

static void drm_copy_string(char *dst, size_t dst_len, const char *src) {
    size_t src_len;
    size_t copy_len;

    if (!dst || !dst_len) {
        return;
    }

    src_len  = strlen(src);
    copy_len = MIN(dst_len, src_len);
    memcpy(dst, src, copy_len);
}

static void drm_fill_display_mode(
    drm_device_t *dev, struct drm_mode_modeinfo *mode, uint32_t width, uint32_t height
) {
    UNUSED(dev);
    memset(mode, 0, sizeof(*mode));
    mode->clock       = width * HZ;
    mode->hdisplay    = width;
    mode->hsync_start = width + 16;
    mode->hsync_end   = width + 16 + 96;
    mode->htotal      = width + 16 + 96 + 48;
    mode->vdisplay    = height;
    mode->vsync_start = height + 10;
    mode->vsync_end   = height + 10 + 2;
    mode->vtotal      = height + 10 + 2 + 33;
    mode->vrefresh    = HZ;
    mode->type        = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
    sprintf(mode->name, "%dx%d", width, height);
}

static void drm_copy_property_enum(
    struct drm_mode_property_enum *dst, uint64_t value, const char *name
) {
    dst->value = value;
    memset(dst->name, 0, sizeof(dst->name));
    drm_copy_string(dst->name, sizeof(dst->name), name);
}

static uint32_t drm_crtc_mode_blob_id(uint32_t crtc_id) {
    return DRM_BLOB_ID_CRTC_MODE_BASE + crtc_id;
}

static uint32_t drm_connector_edid_blob_id(uint32_t connector_id) {
    return DRM_BLOB_ID_CONNECTOR_EDID_BASE + connector_id;
}

static uint32_t drm_plane_in_formats_blob_id(uint32_t plane_id) {
    return DRM_BLOB_ID_PLANE_IN_FORMATS_BASE + plane_id;
}

static bool drm_mode_blob_to_crtc_id(uint32_t blob_id, uint32_t *crtc_id) {
    if (blob_id <= DRM_BLOB_ID_CRTC_MODE_BASE || blob_id >= DRM_BLOB_ID_CONNECTOR_EDID_BASE) {
        return false;
    }

    *crtc_id = blob_id - DRM_BLOB_ID_CRTC_MODE_BASE;
    return *crtc_id != 0;
}

static bool drm_blob_to_connector_edid_id(uint32_t blob_id, uint32_t *connector_id) {
    if (blob_id <= DRM_BLOB_ID_CONNECTOR_EDID_BASE || blob_id >= DRM_BLOB_ID_PLANE_IN_FORMATS_BASE) {
        return false;
    }

    *connector_id = blob_id - DRM_BLOB_ID_CONNECTOR_EDID_BASE;
    return *connector_id != 0;
}

static bool drm_blob_to_plane_in_formats_id(uint32_t blob_id, uint32_t *plane_id) {
    if (blob_id <= DRM_BLOB_ID_PLANE_IN_FORMATS_BASE || blob_id >= DRM_BLOB_ID_USER_BASE) {
        return false;
    }

    *plane_id = blob_id - DRM_BLOB_ID_PLANE_IN_FORMATS_BASE;
    return *plane_id != 0;
}

static ssize_t drm_user_blob_find_index_locked(drm_device_t *dev, uint32_t blob_id) {
    for (int i = 0; i < DRM_MAX_USER_BLOBS; i++) {
        if (drm_user_blobs[i].used && drm_user_blobs[i].dev == dev
            && drm_user_blobs[i].blob_id == blob_id) {
            return i;
        }
    }

    return -1;
}

static int drm_user_blob_generate_id_locked(uint32_t *blob_id) {
    uint32_t candidate = drm_user_blob_next_id;
    if (candidate <= DRM_BLOB_ID_USER_BASE || candidate > DRM_BLOB_ID_USER_LAST) {
        candidate = DRM_BLOB_ID_USER_BASE + 1;
    }

    uint32_t id_space = DRM_BLOB_ID_USER_LAST - DRM_BLOB_ID_USER_BASE;
    for (uint32_t tries = 0; tries < id_space; tries++) {
        bool exists = false;
        for (int i = 0; i < DRM_MAX_USER_BLOBS; i++) {
            if (drm_user_blobs[i].used && drm_user_blobs[i].blob_id == candidate) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            *blob_id             = candidate;
            drm_user_blob_next_id = candidate + 1;
            if (drm_user_blob_next_id > DRM_BLOB_ID_USER_LAST) {
                drm_user_blob_next_id = DRM_BLOB_ID_USER_BASE + 1;
            }
            return 0;
        }

        candidate++;
        if (candidate > DRM_BLOB_ID_USER_LAST) {
            candidate = DRM_BLOB_ID_USER_BASE + 1;
        }
    }

    return -ENOSPC;
}

static void drm_fill_crtc_modeinfo(
    drm_device_t *dev, drm_crtc_t *crtc, struct drm_mode_modeinfo *mode
) {
    if (crtc && crtc->mode_valid && crtc->mode.hdisplay > 0 && crtc->mode.vdisplay > 0) {
        memcpy(mode, &crtc->mode, sizeof(*mode));
        return;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bpp = 0;
    memset(mode, 0, sizeof(*mode));

    if (dev->op->get_display_info && dev->op->get_display_info(dev, &width, &height, &bpp) == 0
        && width > 0 && height > 0) {
        drm_fill_display_mode(dev, mode, width, height);
        return;
    }

    strcpy(mode->name, "unknown");
}

static void drm_edid_set_descriptor_text(uint8_t *desc, uint8_t tag, const char *text) {
    memset(desc, 0, 18);
    desc[3] = tag;
    desc[4] = 0x00;

    size_t i = 0;
    for (; i < 13 && text[i]; i++) {
        desc[5 + i] = (uint8_t)text[i];
    }
    if (i < 13) {
        desc[5 + i++] = '\n';
    }
    for (; i < 13; i++) {
        desc[5 + i] = ' ';
    }
}

static void drm_edid_fill_dtd(
    uint8_t *dtd, uint32_t width, uint32_t height, uint32_t mm_width, uint32_t mm_height,
    uint32_t refresh_hz
) {
    uint32_t hblank            = 160;
    uint32_t vblank            = 45;
    uint32_t hsync_offset      = 48;
    uint32_t hsync_pulse       = 32;
    uint32_t vsync_offset      = 3;
    uint32_t vsync_pulse       = 5;
    uint32_t htotal            = width + hblank;
    uint32_t vtotal            = height + vblank;
    uint32_t pixel_clock_10khz = (htotal * vtotal * refresh_hz) / 10000U;

    memset(dtd, 0, 18);
    dtd[0]  = pixel_clock_10khz & 0xff;
    dtd[1]  = (pixel_clock_10khz >> 8) & 0xff;
    dtd[2]  = width & 0xff;
    dtd[3]  = hblank & 0xff;
    dtd[4]  = ((width >> 8) & 0xf) << 4 | ((hblank >> 8) & 0xf);
    dtd[5]  = height & 0xff;
    dtd[6]  = vblank & 0xff;
    dtd[7]  = ((height >> 8) & 0xf) << 4 | ((vblank >> 8) & 0xf);
    dtd[8]  = hsync_offset & 0xff;
    dtd[9]  = hsync_pulse & 0xff;
    dtd[10] = ((vsync_offset & 0xf) << 4) | (vsync_pulse & 0xf);
    dtd[11] = ((hsync_offset >> 8) & 0x3) << 6 | ((hsync_pulse >> 8) & 0x3) << 4
            | ((vsync_offset >> 4) & 0x3) << 2 | ((vsync_pulse >> 4) & 0x3);
    dtd[12] = mm_width & 0xff;
    dtd[13] = mm_height & 0xff;
    dtd[14] = ((mm_width >> 8) & 0xf) << 4 | ((mm_height >> 8) & 0xf);
    dtd[17] = 0x1a;
}

static void drm_edid_set_default_chromaticity(uint8_t edid[128]) {
    const uint16_t red_x   = 655;
    const uint16_t red_y   = 338;
    const uint16_t green_x = 307;
    const uint16_t green_y = 614;
    const uint16_t blue_x  = 154;
    const uint16_t blue_y  = 61;
    const uint16_t white_x = 320;
    const uint16_t white_y = 337;

    edid[25] = ((red_x & 0x3) << 6) | ((red_y & 0x3) << 4) | ((green_x & 0x3) << 2)
             | (green_y & 0x3);
    edid[26] = ((blue_x & 0x3) << 6) | ((blue_y & 0x3) << 4) | ((white_x & 0x3) << 2)
             | (white_y & 0x3);
    edid[27] = (uint8_t)(red_x >> 2);
    edid[28] = (uint8_t)(red_y >> 2);
    edid[29] = (uint8_t)(green_x >> 2);
    edid[30] = (uint8_t)(green_y >> 2);
    edid[31] = (uint8_t)(blue_x >> 2);
    edid[32] = (uint8_t)(blue_y >> 2);
    edid[33] = (uint8_t)(white_x >> 2);
    edid[34] = (uint8_t)(white_y >> 2);
}

static void drm_build_connector_edid(drm_device_t *dev, drm_connector_t *conn, uint8_t edid[128]) {
    uint32_t width     = 1024;
    uint32_t height    = 768;
    uint32_t refresh   = 60;
    uint32_t mm_width  = conn->mm_width;
    uint32_t mm_height = conn->mm_height;

    if (conn->modes && conn->count_modes > 0) {
        if (conn->modes[0].hdisplay > 0) {
            width = conn->modes[0].hdisplay;
        }
        if (conn->modes[0].vdisplay > 0) {
            height = conn->modes[0].vdisplay;
        }
        if (conn->modes[0].vrefresh > 0) {
            refresh = conn->modes[0].vrefresh;
        }
    } else if (dev->op->get_display_info) {
        uint32_t bpp = 0;
        if (dev->op->get_display_info(dev, &width, &height, &bpp) != 0) {
            width  = 1024;
            height = 768;
        }
    }

    if (mm_width == 0) {
        mm_width = (width * 264U) / 1000U;
        if (mm_width == 0) {
            mm_width = 1;
        }
    }
    if (mm_height == 0) {
        mm_height = (height * 264U) / 1000U;
        if (mm_height == 0) {
            mm_height = 1;
        }
    }

    memset(edid, 0, 128);
    edid[0]   = 0x00;
    edid[1]   = 0xff;
    edid[2]   = 0xff;
    edid[3]   = 0xff;
    edid[4]   = 0xff;
    edid[5]   = 0xff;
    edid[6]   = 0xff;
    edid[7]   = 0x00;
    edid[8]   = 0x38;
    edid[9]   = 0x2f;
    edid[10]  = 0x01;
    edid[11]  = 0x00;
    edid[12]  = 0x01;
    edid[13]  = 0x00;
    edid[14]  = 0x00;
    edid[15]  = 0x00;
    edid[16]  = 0x01;
    edid[17]  = 34;
    edid[18]  = 0x01;
    edid[19]  = 0x04;
    edid[20]  = 0x80;
    edid[21]  = width & 0xff;
    edid[22]  = height & 0xff;
    edid[23]  = 0x78;
    edid[24]  = 0x0a;

    drm_edid_set_default_chromaticity(edid);

    for (int i = 38; i < 54; i++) {
        edid[i] = 0x01;
    }

    drm_edid_fill_dtd(&edid[54], width, height, mm_width, mm_height, refresh);
    drm_edid_set_descriptor_text(&edid[72], 0xfc, "CoolPotOS DRM");
    drm_edid_set_descriptor_text(&edid[90], 0xff, "00000001");
    drm_edid_set_descriptor_text(&edid[108], 0xfe, "plainfb");

    edid[126] = 0;

    uint8_t sum = 0;
    for (int i = 0; i < 127; i++) {
        sum += edid[i];
    }
    edid[127] = (uint8_t)(0x100 - sum);
}

static size_t drm_fill_plane_in_formats_blob(drm_plane_t *plane, uint8_t *blob, size_t blob_size) {
    size_t formats_size = (size_t)plane->count_format_types * sizeof(uint32_t);
    size_t total_size   = sizeof(struct drm_format_modifier_blob) + formats_size;
    struct drm_format_modifier_blob header = {
        .version          = FORMAT_BLOB_CURRENT,
        .flags            = 0,
        .count_formats    = plane->count_format_types,
        .formats_offset   = sizeof(struct drm_format_modifier_blob),
        .count_modifiers  = 0,
        .modifiers_offset = sizeof(struct drm_format_modifier_blob) + formats_size,
    };

    if (!blob || blob_size < sizeof(header)) {
        return total_size;
    }

    memcpy(blob, &header, sizeof(header));
    if (plane->format_types && plane->count_format_types > 0 && blob_size >= total_size) {
        memcpy(blob + header.formats_offset, plane->format_types, formats_size);
    }

    return total_size;
}

static void drm_import_resource_id(drm_device_t *dev, uint32_t obj_id) {
    if (!dev || obj_id == 0) {
        return;
    }

    if (obj_id >= dev->resource_mgr.next_object_id) {
        dev->resource_mgr.next_object_id = obj_id + 1;
    }
}

static int drm_mode_resolve_obj_type(drm_device_t *dev, uint32_t obj_id, uint32_t *obj_type) {
    uint32_t resolved_type = DRM_MODE_OBJECT_ANY;

    for (int idx = 0; idx < DRM_MAX_CONNECTORS_PER_DEVICE; idx++) {
        if (!dev->resource_mgr.connectors[idx] || dev->resource_mgr.connectors[idx]->id != obj_id) {
            continue;
        }

        if (resolved_type != DRM_MODE_OBJECT_ANY && resolved_type != DRM_MODE_OBJECT_CONNECTOR) {
            return -EINVAL;
        }
        resolved_type = DRM_MODE_OBJECT_CONNECTOR;
        break;
    }

    for (int idx = 0; idx < DRM_MAX_ENCODERS_PER_DEVICE; idx++) {
        if (!dev->resource_mgr.encoders[idx] || dev->resource_mgr.encoders[idx]->id != obj_id) {
            continue;
        }

        if (resolved_type != DRM_MODE_OBJECT_ANY && resolved_type != DRM_MODE_OBJECT_ENCODER) {
            return -EINVAL;
        }
        resolved_type = DRM_MODE_OBJECT_ENCODER;
        break;
    }

    for (int idx = 0; idx < DRM_MAX_CRTCS_PER_DEVICE; idx++) {
        if (!dev->resource_mgr.crtcs[idx] || dev->resource_mgr.crtcs[idx]->id != obj_id) {
            continue;
        }

        if (resolved_type != DRM_MODE_OBJECT_ANY && resolved_type != DRM_MODE_OBJECT_CRTC) {
            return -EINVAL;
        }
        resolved_type = DRM_MODE_OBJECT_CRTC;
        break;
    }

    for (int idx = 0; idx < DRM_MAX_FRAMEBUFFERS_PER_DEVICE; idx++) {
        if (!dev->resource_mgr.framebuffers[idx] || dev->resource_mgr.framebuffers[idx]->id != obj_id) {
            continue;
        }

        if (resolved_type != DRM_MODE_OBJECT_ANY && resolved_type != DRM_MODE_OBJECT_FB) {
            return -EINVAL;
        }
        resolved_type = DRM_MODE_OBJECT_FB;
        break;
    }

    for (int idx = 0; idx < DRM_MAX_PLANES_PER_DEVICE; idx++) {
        if (!dev->resource_mgr.planes[idx] || dev->resource_mgr.planes[idx]->id != obj_id) {
            continue;
        }

        if (resolved_type != DRM_MODE_OBJECT_ANY && resolved_type != DRM_MODE_OBJECT_PLANE) {
            return -EINVAL;
        }
        resolved_type = DRM_MODE_OBJECT_PLANE;
        break;
    }

    if (resolved_type == DRM_MODE_OBJECT_ANY) {
        return -ENOENT;
    }

    *obj_type = resolved_type;
    return 0;
}

typedef enum drm_sysfs_attr {
    DRM_SYSFS_ATTR_VERSION,
    DRM_SYSFS_ATTR_DEV,
    DRM_SYSFS_ATTR_MODES,
    DRM_SYSFS_ATTR_UEVENT,
} drm_sysfs_attr_t;

typedef struct drm_sysfs_file {
    sysfs_handle_t handle;
    drm_device_t *dev;
    drm_sysfs_attr_t attr;
} drm_sysfs_file_t;

static char *drm_sysfs_join_path(vfs_node_t parent, const char *name) {
    if (parent == NULL || name == NULL) {
        return NULL;
    }

    char *base = vfs_get_fullpath(parent);
    if (base == NULL) {
        return NULL;
    }

    size_t base_len = strlen(base);
    size_t name_len = strlen(name);
    size_t path_len = base_len + name_len + 2;
    char *path      = malloc(path_len);
    if (path == NULL) {
        free(base);
        return NULL;
    }

    if (strcmp(base, "/") == 0) {
        sprintf(path, "/%s", name);
    } else {
        sprintf(path, "%s/%s", base, name);
    }

    free(base);
    return path;
}

static vfs_node_t drm_sysfs_lookup_child(vfs_node_t parent, const char *name) {
    char *path = drm_sysfs_join_path(parent, name);
    if (path == NULL) {
        return NULL;
    }

    vfs_node_t node = vfs_open(path);
    free(path);
    return node;
}

static void drm_sysfs_release_handle(vfs_node_t node) {
    if (node == NULL || node->handle == NULL) {
        return;
    }

    sysfs_header_t *header = node->handle;
    if (header->type == SYSFS_NONE) {
        sysfs_handle_t *handle = node->handle;
        if (handle->data != NULL) {
            free(handle->data);
        }
    }

    free(node->handle);
    node->handle = NULL;
}

static size_t drm_sysfs_format_modes_text(
    drm_device_t *drm_dev, char *buffer, size_t buffer_size
) {
    uint32_t width  = 1024;
    uint32_t height = 768;
    uint32_t bpp    = 32;

    for (size_t i = 0; i < DRM_MAX_CONNECTORS_PER_DEVICE; i++) {
        drm_connector_t *connector = drm_dev->resource_mgr.connectors[i];
        if (connector == NULL || connector->modes == NULL || connector->count_modes == 0) {
            continue;
        }

        width  = connector->modes[0].hdisplay;
        height = connector->modes[0].vdisplay;
        goto out;
    }

    if (drm_dev->op && drm_dev->op->get_display_info) {
        drm_dev->op->get_display_info(drm_dev, &width, &height, &bpp);
    }

out:
    snprintf(buffer, buffer_size, "%ux%u\n", width, height);
    return strlen(buffer);
}

static size_t drm_sysfs_format_dev_text(drm_device_t *drm_dev, char *buffer, size_t buffer_size) {
    int major = (drm_dev->dev_nr >> 8) & 0xff;
    int minor = drm_dev->dev_nr & 0xff;

    snprintf(buffer, buffer_size, "%d:%d\n", major, minor);
    return strlen(buffer);
}

static size_t drm_sysfs_format_uevent_text(
    drm_device_t *drm_dev, char *buffer, size_t buffer_size
) {
    int minor = drm_dev->dev_nr & 0xff;
    int major = (drm_dev->dev_nr >> 8) & 0xff;
    char dev_name[32];

    snprintf(dev_name, sizeof(dev_name), "card%d", minor);
    snprintf(
        buffer,
        buffer_size,
        "MAJOR=%d\nMINOR=%d\nDEVNAME=dri/%s\nDEVTYPE=drm_minor\nSUBSYSTEM=drm\n",
        major,
        minor,
        dev_name
    );
    return strlen(buffer);
}

static size_t drm_sysfs_format_attr(
    drm_sysfs_file_t *handle, char *buffer, size_t buffer_size
) {
    if (handle == NULL || buffer == NULL || buffer_size == 0) {
        return 0;
    }

    switch (handle->attr) {
    case DRM_SYSFS_ATTR_VERSION:
        snprintf(buffer, buffer_size, "drm 1.1.0 20060810\n");
        return strlen(buffer);
    case DRM_SYSFS_ATTR_DEV:
        return drm_sysfs_format_dev_text(handle->dev, buffer, buffer_size);
    case DRM_SYSFS_ATTR_MODES:
        return drm_sysfs_format_modes_text(handle->dev, buffer, buffer_size);
    case DRM_SYSFS_ATTR_UEVENT:
        return drm_sysfs_format_uevent_text(handle->dev, buffer, buffer_size);
    default:
        return 0;
    }
}

static size_t drm_sysfs_attr_read(void *file, void *addr, size_t offset, size_t size) {
    drm_sysfs_file_t *handle = file;
    if (handle == NULL || handle->dev == NULL || addr == NULL) {
        return 0;
    }

    char content[256];
    size_t len = drm_sysfs_format_attr(handle, content, sizeof(content));
    if (len == 0 || offset >= len) {
        return 0;
    }

    size_t actual = len - offset;
    if (actual > size) {
        actual = size;
    }

    memcpy(addr, content + offset, actual);
    return actual;
}

static size_t drm_sysfs_attr_write(void *file, const void *addr, size_t offset, size_t size) {
    drm_sysfs_file_t *handle = file;
    UNUSED(addr, offset);

    if (handle == NULL) {
        return 0;
    }

    return handle->attr == DRM_SYSFS_ATTR_UEVENT ? size : 0;
}

static void drm_sysfs_install_attr_file(
    vfs_node_t parent, const char *name, drm_device_t *drm_dev, drm_sysfs_attr_t attr
) {
    vfs_node_t node = drm_sysfs_lookup_child(parent, name);
    if (node == NULL) {
        node = sysfs_child_append(parent, name, SYSFS_NONE);
    }
    if (node == NULL) {
        return;
    }

    drm_sysfs_release_handle(node);

    drm_sysfs_file_t *handle = calloc(1, sizeof(drm_sysfs_file_t));
    strncpy(handle->handle.name, name, sizeof(handle->handle.name) - 1);
    handle->handle.header.node  = node;
    handle->handle.header.type  = SYSFS_NONE;
    handle->handle.header.read  = drm_sysfs_attr_read;
    handle->handle.header.write = drm_sysfs_attr_write;
    handle->dev                 = drm_dev;
    handle->attr                = attr;

    node->handle = handle;
    char content[256];
    node->size = drm_sysfs_format_attr(handle, content, sizeof(content));
    node->mode   = attr == DRM_SYSFS_ATTR_UEVENT ? 0644 : 0444;
}

static void drm_sysfs_ensure_symlink(vfs_node_t parent, const char *name, const char *target) {
    if (parent == NULL || name == NULL || target == NULL) {
        return;
    }

    if (drm_sysfs_lookup_child(parent, name) != NULL) {
        return;
    }

    sysfs_child_append_symlink(parent, name, target);
}

static void drm_sysfs_ensure_symlink_node(vfs_node_t parent, const char *name, vfs_node_t target) {
    if (parent == NULL || name == NULL || target == NULL) {
        return;
    }

    if (drm_sysfs_lookup_child(parent, name) != NULL) {
        return;
    }

    sysfs_child_append_symlink_node(parent, name, target);
}

static vfs_node_t drm_sysfs_get_device_root(drm_device_t *drm_dev, const char *dev_name) {
    if (drm_dev->pci_dev != NULL) {
        return sysfs_get_pci_device_node(
            drm_dev->pci_dev->segment,
            drm_dev->pci_dev->bus,
            drm_dev->pci_dev->slot,
            drm_dev->pci_dev->func
        );
    }

    vfs_node_t system_root = sysfs_ensure_dir(sysfs_get_devices_root(), "system");
    if (system_root == NULL) {
        return NULL;
    }

    vfs_node_t display_root = sysfs_ensure_dir(system_root, "display");
    if (display_root == NULL) {
        return NULL;
    }

    char device_name[48];
    snprintf(device_name, sizeof(device_name), "%s-device", dev_name);
    return sysfs_ensure_dir(display_root, device_name);
}

static void drm_sysfs_register_device(drm_device_t *drm_dev) {
    vfs_node_t device_dir;
    vfs_node_t drm_dir;
    vfs_node_t card_dir;
    vfs_node_t class_drm_dir;
    char dev_name[32];
    char content[128];
    int major;
    int minor;

    if (!drm_dev) {
        return;
    }

    major = (drm_dev->dev_nr >> 8) & 0xff;
    minor = drm_dev->dev_nr & 0xff;
    snprintf(dev_name, sizeof(dev_name), "card%d", minor);

    device_dir = drm_sysfs_get_device_root(drm_dev, dev_name);
    if (!device_dir) {
        return;
    }

    drm_dir = sysfs_ensure_dir(device_dir, "drm");
    if (!drm_dir) {
        return;
    }

    card_dir = sysfs_ensure_dir(drm_dir, dev_name);
    if (!card_dir) {
        return;
    }

    drm_sysfs_install_attr_file(drm_dir, "version", drm_dev, DRM_SYSFS_ATTR_VERSION);
    drm_sysfs_install_attr_file(card_dir, "dev", drm_dev, DRM_SYSFS_ATTR_DEV);
    drm_sysfs_install_attr_file(card_dir, "modes", drm_dev, DRM_SYSFS_ATTR_MODES);
    drm_sysfs_install_attr_file(card_dir, "uevent", drm_dev, DRM_SYSFS_ATTR_UEVENT);
    drm_sysfs_ensure_symlink(card_dir, "subsystem", "/sys/class/drm");
    drm_sysfs_ensure_symlink_node(card_dir, "device", device_dir);

    class_drm_dir = sysfs_ensure_dir(sysfs_get_class_root(), "drm");
    if (class_drm_dir) {
        drm_sysfs_ensure_symlink_node(class_drm_dir, dev_name, card_dir);
    }

    drm_sysfs_format_uevent_text(drm_dev, content, sizeof(content));
    char *card_path = vfs_get_fullpath(card_dir);
    if (card_path != NULL) {
        sysfs_regist_dev('c', major, minor, card_path, dev_name, content);
        free(card_path);
    }
}

void drm_sysfs_populate() {
    const drmd_device_t *device = NULL;

    cow_foreach(drm_devices_get(), device) {
        drm_sysfs_register_device(device->ptr);
    }
}

void drm_device_set_driver_info(
    drm_device_t *dev, const char *name, const char *date, const char *desc
) {
    if (!dev) {
        return;
    }

    if (!name || !name[0]) {
        name = DRM_NAME;
    }
    if (!date || !date[0]) {
        date = "20060810";
    }
    if (!desc || !desc[0]) {
        desc = "CoolPotOS DRM";
    }

    strncpy(dev->driver_name, name, sizeof(dev->driver_name) - 1);
    strncpy(dev->driver_date, date, sizeof(dev->driver_date) - 1);
    strncpy(dev->driver_desc, desc, sizeof(dev->driver_desc) - 1);
}

int drm_post_event(drm_device_t *dev, uint32_t type, uint64_t user_data) {
    if (!dev) {
        return -ENODEV;
    }

    struct k_drm_event *event = malloc(sizeof(struct k_drm_event));
    if (!event) {
        return -ENOMEM;
    }

    uint64_t now            = nano_time();
    event->type             = type;
    event->user_data        = user_data;
    event->timestamp.tv_sec = now / 1000000000ULL;
    event->timestamp.tv_nsec = now % 1000000000ULL;

    spin_lock(dev->event_lock);
    if (type == DRM_EVENT_VBLANK) {
        dev->vblank_counter++;
    }

    int slot = -1;
    for (int i = 0; i < DRM_MAX_EVENTS_COUNT; i++) {
        if (!dev->drm_events[i]) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        free(dev->drm_events[0]);
        memmove(
            &dev->drm_events[0],
            &dev->drm_events[1],
            sizeof(struct k_drm_event *) * (DRM_MAX_EVENTS_COUNT - 1)
        );
        slot = DRM_MAX_EVENTS_COUNT - 1;
    }

    dev->drm_events[slot] = event;
    spin_unlock(dev->event_lock);
    return 0;
}

int drm_defer_event(drm_device_t *dev, uint32_t type, uint64_t user_data) {
    return drm_post_event(dev, type, user_data);
}

size_t drm_ioctl(void *data, size_t cmd, size_t arg) {
    drm_device_t *dev = (drm_device_t *)data;
    uint32_t drm_cmd  = cmd & 0xffffffff;

    switch (drm_cmd) {
    case DRM_IOCTL_VERSION: {
        struct drm_version *version = (struct drm_version *)arg;
        const char *driver_name = dev->driver_name[0] ? dev->driver_name : DRM_NAME;
        const char *driver_date = dev->driver_date[0] ? dev->driver_date : "20060810";
        const char *driver_desc = dev->driver_desc[0] ? dev->driver_desc : "CoolPotOS DRM";

        version->version_major      = 1;
        version->version_minor      = 0;
        version->version_patchlevel = 0;
        version->name_len           = strlen(driver_name);
        version->date_len           = strlen(driver_date);
        version->desc_len           = strlen(driver_desc);

        drm_copy_string(version->name, version->name_len, driver_name);
        drm_copy_string(version->date, version->date_len, driver_date);
        drm_copy_string(version->desc, version->desc_len, driver_desc);
        return 0;
    }

    case DRM_IOCTL_GET_CAP: {
        struct drm_get_cap *cap = (struct drm_get_cap *)arg;
        switch (cap->capability) {
        case DRM_CAP_DUMB_BUFFER:
            cap->value = 1;
            return 0;
        case DRM_CAP_DUMB_PREFERRED_DEPTH:
            cap->value = 24;
            return 0;
        case DRM_CAP_CRTC_IN_VBLANK_EVENT:
            cap->value = 1;
            return 0;
        case DRM_CAP_TIMESTAMP_MONOTONIC:
            cap->value = 1;
            return 0;
        case DRM_CAP_CURSOR_WIDTH:
            cap->value = 32;
            return 0;
        case DRM_CAP_CURSOR_HEIGHT:
            cap->value = 32;
            return 0;
        case DRM_CAP_ADDFB2_MODIFIERS:
            cap->value = 0;
            return 0;
        case DRM_CAP_DUMB_PREFER_SHADOW:
            cap->value = 1;
            return 0;
        case DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP:
            cap->value = 1;
            return 0;
        default:
            printk("drm: Unsupported capability %d\n", cap->capability);
            cap->value = 0;
            return 0;
        }
    }

    case DRM_IOCTL_MODE_GETRESOURCES: {
        struct drm_mode_card_res *res = (struct drm_mode_card_res *)arg;
        uint32_t req_fbs        = res->count_fbs;
        uint32_t req_crtcs      = res->count_crtcs;
        uint32_t req_connectors = res->count_connectors;
        uint32_t req_encoders   = res->count_encoders;
        uint32_t count_fbs      = 0;
        uint32_t count_crtcs    = 0;
        uint32_t count_connectors;
        uint32_t count_encoders;

        // Count framebuffers
        for (uint32_t i = 0; i < DRM_MAX_FRAMEBUFFERS_PER_DEVICE; i++) {
            if (dev->resource_mgr.framebuffers[i]) {
                count_fbs++;
            }
        }

        // Count CRTCs
        for (uint32_t i = 0; i < DRM_MAX_CRTCS_PER_DEVICE; i++) {
            if (dev->resource_mgr.crtcs[i]) {
                count_crtcs++;
            }
        }

        // Count connectors
        count_connectors = 0;
        for (uint32_t i = 0; i < DRM_MAX_CONNECTORS_PER_DEVICE; i++) {
            if (dev->resource_mgr.connectors[i]) {
                count_connectors++;
            }
        }

        // Count encoders
        count_encoders = 0;
        for (uint32_t i = 0; i < DRM_MAX_ENCODERS_PER_DEVICE; i++) {
            if (dev->resource_mgr.encoders[i]) {
                count_encoders++;
            }
        }

        res->count_fbs        = count_fbs;
        res->count_crtcs      = count_crtcs;
        res->count_connectors = count_connectors;
        res->count_encoders   = count_encoders;

        uint32_t width, height, bpp;
        dev->op->get_display_info(dev, &width, &height, &bpp);

        res->min_width  = width;
        res->min_height = height;
        res->max_width  = width;
        res->max_height = height;
        // Fill encoder IDs if pointer provided
        if (res->encoder_id_ptr && req_encoders > 0) {
            uint32_t *encoder_ids = (uint32_t *)(uintptr_t)res->encoder_id_ptr;
            uint32_t idx          = 0;
            for (uint32_t i = 0; i < DRM_MAX_ENCODERS_PER_DEVICE; i++) {
                if (dev->resource_mgr.encoders[i] && idx < req_encoders) {
                    encoder_ids[idx++] = dev->resource_mgr.encoders[i]->id;
                }
            }
        }

        // Fill CRTC IDs if pointer provided
        if (res->crtc_id_ptr && req_crtcs > 0) {
            uint32_t *crtc_ids = (uint32_t *)(uintptr_t)res->crtc_id_ptr;
            uint32_t idx       = 0;
            for (uint32_t i = 0; i < DRM_MAX_CRTCS_PER_DEVICE; i++) {
                if (dev->resource_mgr.crtcs[i] && idx < req_crtcs) {
                    crtc_ids[idx++] = dev->resource_mgr.crtcs[i]->id;
                }
            }
        }

        // Fill connector IDs if pointer provided
        if (res->connector_id_ptr && req_connectors > 0) {
            uint32_t *connector_ids = (uint32_t *)(uintptr_t)res->connector_id_ptr;
            uint32_t idx            = 0;
            for (uint32_t i = 0; i < DRM_MAX_CONNECTORS_PER_DEVICE; i++) {
                if (dev->resource_mgr.connectors[i] && idx < req_connectors) {
                    connector_ids[idx++] = dev->resource_mgr.connectors[i]->id;
                }
            }
        }

        // Fill framebuffer IDs if pointer provided
        if (res->fb_id_ptr && req_fbs > 0) {
            uint32_t *fb_ids = (uint32_t *)(uintptr_t)res->fb_id_ptr;
            uint32_t idx     = 0;
            for (uint32_t i = 0; i < DRM_MAX_FRAMEBUFFERS_PER_DEVICE; i++) {
                if (dev->resource_mgr.framebuffers[i] && idx < req_fbs) {
                    fb_ids[idx++] = dev->resource_mgr.framebuffers[i]->id;
                }
            }
        }
        return 0;
    }

    case DRM_IOCTL_MODE_GETCRTC: {
        struct drm_mode_crtc *crtc = (struct drm_mode_crtc *)arg;

        // Find the CRTC by ID
        drm_crtc_t *crtc_obj = drm_crtc_get(&dev->resource_mgr, crtc->crtc_id);
        if (!crtc_obj) {
            return -ENOENT;
        }

        struct drm_mode_modeinfo mode;
        drm_fill_crtc_modeinfo(dev, crtc_obj, &mode);

        crtc->gamma_size = 0;
        crtc->mode_valid = 1;
        memcpy(&crtc->mode, &mode, sizeof(struct drm_mode_modeinfo));
        crtc->fb_id = crtc_obj->fb_id;
        crtc->x     = crtc_obj->x;
        crtc->y     = crtc_obj->y;

        // Release reference
        drm_crtc_free(&dev->resource_mgr, crtc_obj->id);
        return 0;
    }

    case DRM_IOCTL_MODE_GETENCODER: {
        struct drm_mode_get_encoder *enc = (struct drm_mode_get_encoder *)arg;

        // Find the encoder by ID
        drm_encoder_t *encoder = drm_encoder_get(&dev->resource_mgr, enc->encoder_id);
        if (!encoder) {
            return -ENOENT;
        }

        enc->encoder_type    = encoder->type;
        enc->crtc_id         = encoder->crtc_id;
        enc->possible_crtcs  = encoder->possible_crtcs;
        enc->possible_clones = encoder->possible_clones;

        // Release reference
        drm_encoder_free(&dev->resource_mgr, encoder->id);
        return 0;
    }

    case DRM_IOCTL_MODE_CREATE_DUMB: {
        return dev->op->create_dumb(dev, (struct drm_mode_create_dumb *)arg);
    }

    case DRM_IOCTL_MODE_MAP_DUMB: {
        return dev->op->map_dumb(dev, (struct drm_mode_map_dumb *)arg);
    }

    case DRM_IOCTL_MODE_DESTROY_DUMB: {
        struct drm_mode_destroy_dumb *destroy = (struct drm_mode_destroy_dumb *)arg;
        if (!dev->op->destroy_dumb || destroy->handle == 0) {
            return -EINVAL;
        }
        return dev->op->destroy_dumb(dev, destroy->handle);
    }

    case DRM_IOCTL_MODE_GETCONNECTOR: {
        struct drm_mode_get_connector *conn = (struct drm_mode_get_connector *)arg;
        uint32_t req_modes;
        uint32_t req_props;
        uint32_t req_encoders;

        // Find the connector by ID
        drm_connector_t *connector = drm_connector_get(&dev->resource_mgr, conn->connector_id);
        if (!connector) {
            return -ENOENT;
        }

        req_modes = conn->count_modes;
        req_props = conn->count_props;
        req_encoders = conn->count_encoders;

        conn->encoder_id        = connector->encoder_id;
        conn->connector_type    = connector->type;
        conn->connector_type_id = 1;
        conn->mm_width          = connector->mm_width;
        conn->mm_height         = connector->mm_height;
        conn->subpixel          = connector->subpixel;
        conn->connection     = connector->connection;
        conn->count_modes    = connector->count_modes;
        conn->count_props    = 3;
        conn->count_encoders = connector->encoder_id ? 1 : 0;

        // Fill modes if pointer provided
        struct drm_mode_modeinfo *mode = (struct drm_mode_modeinfo *)(uintptr_t)conn->modes_ptr;
        if (mode && connector->modes && req_modes > 0) {
            memcpy(
                mode,
                connector->modes,
                MIN(req_modes, connector->count_modes) * sizeof(struct drm_mode_modeinfo)
            );
        }

        // Fill encoders if pointer provided
        uint32_t *encoders = (uint32_t *)(uintptr_t)conn->encoders_ptr;
        if (encoders && req_encoders > 0 && conn->count_encoders > 0) {
            encoders[0] = connector->encoder_id;
        }

        // Fill properties if pointers provided
        if (conn->props_ptr && conn->prop_values_ptr && req_props > 0) {
            uint32_t *prop_ids    = (uint32_t *)(uintptr_t)conn->props_ptr;
            uint64_t *prop_values = (uint64_t *)(uintptr_t)conn->prop_values_ptr;
            uint32_t copy_props   = MIN(req_props, 3U);
            if (copy_props > 0) {
                prop_ids[0]    = DRM_CONNECTOR_DPMS_PROP_ID;
                prop_values[0] = DRM_MODE_DPMS_ON;
            }
            if (copy_props > 1) {
                prop_ids[1]    = DRM_CONNECTOR_EDID_PROP_ID;
                prop_values[1] = drm_connector_edid_blob_id(connector->id);
            }
            if (copy_props > 2) {
                prop_ids[2]    = DRM_CONNECTOR_CRTC_ID_PROP_ID;
                prop_values[2] = connector->crtc_id;
            }
        }

        // Release reference
        drm_connector_free(&dev->resource_mgr, connector->id);
        return 0;
    }
    case DRM_IOCTL_MODE_GETFB: {
        struct drm_mode_fb_cmd *fb_cmd = (struct drm_mode_fb_cmd *)arg;

        // Find the framebuffer by ID
        drm_framebuffer_t *fb = drm_framebuffer_get(&dev->resource_mgr, fb_cmd->fb_id);
        if (!fb) {
            return -ENOENT;
        }

        fb_cmd->width  = fb->width;
        fb_cmd->height = fb->height;
        fb_cmd->pitch  = fb->pitch;
        fb_cmd->bpp    = fb->bpp;
        fb_cmd->depth  = fb->depth;
        fb_cmd->handle = fb->handle;

        // Release reference
        drm_framebuffer_free(&dev->resource_mgr, fb->id);
        return 0;
    }
    case DRM_IOCTL_MODE_ADDFB: {
        struct drm_mode_fb_cmd *fb_cmd = (struct drm_mode_fb_cmd *)arg;

        return dev->op->add_fb(dev, fb_cmd);
    }
    case DRM_IOCTL_MODE_ADDFB2: {
        struct drm_mode_fb_cmd2 *fb_cmd = (struct drm_mode_fb_cmd2 *)arg;

        return dev->op->add_fb2(dev, fb_cmd);
    }
    case DRM_IOCTL_MODE_RMFB: {
        return 0;
    }

    case DRM_IOCTL_MODE_CLOSEFB: {
        struct drm_mode_closefb *closefb = (struct drm_mode_closefb *)arg;
        if (!closefb || closefb->fb_id == 0 || closefb->pad != 0) {
            return -EINVAL;
        }
        return drm_framebuffer_close(dev, closefb->fb_id);
    }

    case DRM_IOCTL_MODE_SETCRTC: {
        struct drm_mode_crtc *crtc_cmd = (struct drm_mode_crtc *)arg;

        // Find the CRTC by ID
        drm_crtc_t *crtc = drm_crtc_get(&dev->resource_mgr, crtc_cmd->crtc_id);
        if (!crtc) {
            return -ENOENT;
        }

        // Update CRTC state
        uint32_t old_fb_id = crtc->fb_id;
        crtc->fb_id = crtc_cmd->fb_id;
        crtc->x     = crtc_cmd->x;
        crtc->y     = crtc_cmd->y;
        if (crtc_cmd->mode_valid) {
            memcpy(&crtc->mode, &crtc_cmd->mode, sizeof(struct drm_mode_modeinfo));
        }

        // Call driver to set CRTC
        int ret = dev->op->set_crtc(dev, crtc_cmd);

        // Release reference
        drm_crtc_free(&dev->resource_mgr, crtc->id);
        if (ret == 0 && old_fb_id != 0 && old_fb_id != crtc_cmd->fb_id) {
            drm_framebuffer_cleanup_closed(dev, old_fb_id);
        }
        return ret;
    }

    case DRM_IOCTL_MODE_GETPLANERESOURCES: {
        struct drm_mode_get_plane_res *res = (struct drm_mode_get_plane_res *)arg;
        uint32_t req_planes                = res->count_planes;

        // Count available planes
        res->count_planes = 0;
        for (uint32_t i = 0; i < DRM_MAX_PLANES_PER_DEVICE; i++) {
            if (dev->resource_mgr.planes[i]) {
                res->count_planes++;
            }
        }

        // Fill plane IDs if pointer provided
        if (res->plane_id_ptr && req_planes > 0) {
            uint32_t *plane_ids = (uint32_t *)(uintptr_t)res->plane_id_ptr;
            uint32_t idx        = 0;
            for (uint32_t i = 0; i < DRM_MAX_PLANES_PER_DEVICE; i++) {
                if (dev->resource_mgr.planes[i] && idx < req_planes) {
                    plane_ids[idx++] = dev->resource_mgr.planes[i]->id;
                }
            }
        }

        return 0;
    }

    case DRM_IOCTL_MODE_GETPLANE: {
        struct drm_mode_get_plane *plane_cmd = (struct drm_mode_get_plane *)arg;

        // Find the plane by ID
        drm_plane_t *plane = drm_plane_get(&dev->resource_mgr, plane_cmd->plane_id);
        if (!plane) {
            return -ENOENT;
        }

        plane_cmd->plane_id           = plane->id;
        plane_cmd->crtc_id            = plane->crtc_id;
        plane_cmd->fb_id              = plane->fb_id;
        plane_cmd->possible_crtcs     = plane->possible_crtcs;
        plane_cmd->gamma_size         = plane->gamma_size;
        plane_cmd->count_format_types = plane->count_format_types;

        // Fill format types if pointer provided
        if (plane_cmd->format_type_ptr && plane->count_format_types > 0 && plane->format_types) {
            uint32_t *formats = (uint32_t *)(uintptr_t)plane_cmd->format_type_ptr;
            uint32_t count     = MIN(plane_cmd->count_format_types, plane->count_format_types);
            plane_cmd->count_format_types = plane->count_format_types;
            for (uint32_t i = 0; i < count; i++) {
                formats[i] = plane->format_types[i];
            }
        } else {
            plane_cmd->count_format_types = plane->count_format_types;
        }

        // Release reference
        drm_plane_free(&dev->resource_mgr, plane->id);
        return 0;
    }

    case DRM_IOCTL_MODE_SETPLANE: {
        struct drm_mode_set_plane *plane_cmd = (struct drm_mode_set_plane *)arg;

        // Find the plane by ID
        drm_plane_t *plane = drm_plane_get(&dev->resource_mgr, plane_cmd->plane_id);
        if (!plane) {
            return -ENOENT;
        }

        // Update plane state
        uint32_t old_fb_id = plane->fb_id;
        plane->crtc_id = plane_cmd->crtc_id;
        plane->fb_id   = plane_cmd->fb_id;

        // Call driver to set plane (if supported)
        if (dev->op->set_plane) {
            int ret = dev->op->set_plane(dev, plane_cmd);
            if (ret != 0) {
                drm_plane_free(&dev->resource_mgr, plane->id);
                return ret;
            }
        }

        // Release reference
        drm_plane_free(&dev->resource_mgr, plane->id);
        if (old_fb_id != 0 && old_fb_id != plane_cmd->fb_id) {
            drm_framebuffer_cleanup_closed(dev, old_fb_id);
        }
        return 0;
    }

    case DRM_IOCTL_MODE_GETPROPERTY: {
        struct drm_mode_get_property *prop = (struct drm_mode_get_property *)arg;
        uint32_t req_values                = prop->count_values;
        uint32_t req_enum_blobs            = prop->count_enum_blobs;

        memset(prop->name, 0, sizeof(prop->name));
        prop->count_values     = 0;
        prop->count_enum_blobs = 0;

        switch (prop->prop_id) {
        case DRM_PROPERTY_ID_FB_ID:
            prop->flags = DRM_MODE_PROP_OBJECT | DRM_MODE_PROP_ATOMIC;
            drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "FB_ID");
            prop->count_values = 1;
            if (prop->values_ptr && req_values > 0) {
                uint64_t *values = (uint64_t *)(uintptr_t)prop->values_ptr;
                values[0]        = DRM_MODE_OBJECT_FB;
            }
            return 0;

        case DRM_PROPERTY_ID_CRTC_ID:
        case DRM_CONNECTOR_CRTC_ID_PROP_ID:
            prop->flags = DRM_MODE_PROP_OBJECT | DRM_MODE_PROP_ATOMIC;
            drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "CRTC_ID");
            prop->count_values = 1;
            if (prop->values_ptr && req_values > 0) {
                uint64_t *values = (uint64_t *)(uintptr_t)prop->values_ptr;
                values[0]        = DRM_MODE_OBJECT_CRTC;
            }
            return 0;

        case DRM_PROPERTY_ID_CRTC_X:
        case DRM_PROPERTY_ID_CRTC_Y:
            prop->flags        = DRM_MODE_PROP_SIGNED_RANGE | DRM_MODE_PROP_ATOMIC;
            prop->count_values = 2;
            drm_copy_string(
                (char *)prop->name,
                DRM_PROP_NAME_LEN,
                prop->prop_id == DRM_PROPERTY_ID_CRTC_X ? "CRTC_X" : "CRTC_Y"
            );
            if (prop->values_ptr && req_values >= 2) {
                uint64_t *values = (uint64_t *)(uintptr_t)prop->values_ptr;
                values[0]        = (uint64_t)(-(1LL << 31));
                values[1]        = (uint64_t)((1LL << 31) - 1);
            }
            return 0;

        case DRM_PROPERTY_ID_SRC_X:
        case DRM_PROPERTY_ID_SRC_Y:
        case DRM_PROPERTY_ID_SRC_W:
        case DRM_PROPERTY_ID_SRC_H:
            prop->flags        = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC;
            prop->count_values = 2;
            if (prop->prop_id == DRM_PROPERTY_ID_SRC_X) {
                drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "SRC_X");
            } else if (prop->prop_id == DRM_PROPERTY_ID_SRC_Y) {
                drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "SRC_Y");
            } else if (prop->prop_id == DRM_PROPERTY_ID_SRC_W) {
                drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "SRC_W");
            } else {
                drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "SRC_H");
            }
            if (prop->values_ptr && req_values >= 2) {
                uint64_t *values = (uint64_t *)(uintptr_t)prop->values_ptr;
                values[0]        = 0;
                values[1]        = UINT32_MAX;
            }
            return 0;

        case DRM_PROPERTY_ID_CRTC_W:
        case DRM_PROPERTY_ID_CRTC_H:
            prop->flags        = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC;
            prop->count_values = 2;
            drm_copy_string(
                (char *)prop->name,
                DRM_PROP_NAME_LEN,
                prop->prop_id == DRM_PROPERTY_ID_CRTC_W ? "CRTC_W" : "CRTC_H"
            );
            if (prop->values_ptr && req_values >= 2) {
                uint64_t *values = (uint64_t *)(uintptr_t)prop->values_ptr;
                values[0]        = 0;
                values[1]        = 8192;
            }
            return 0;

        case DRM_PROPERTY_ID_PLANE_TYPE:
            prop->flags = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_IMMUTABLE;
            drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "type");
            prop->count_enum_blobs = 3;

            if (prop->enum_blob_ptr && req_enum_blobs > 0) {
                struct drm_mode_property_enum *enums =
                    (struct drm_mode_property_enum *)prop->enum_blob_ptr;
                if (req_enum_blobs > 0) {
                    drm_copy_property_enum(&enums[0], DRM_PLANE_TYPE_PRIMARY, "Primary");
                }
                if (req_enum_blobs > 1) {
                    drm_copy_property_enum(&enums[1], DRM_PLANE_TYPE_OVERLAY, "Overlay");
                }
                if (req_enum_blobs > 2) {
                    drm_copy_property_enum(&enums[2], DRM_PLANE_TYPE_CURSOR, "Cursor");
                }
            }
            return 0;

        case DRM_CRTC_MODE_ID_PROP_ID:
            prop->flags = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC;
            drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "MODE_ID");
            return 0;

        case DRM_CRTC_ACTIVE_PROP_ID:
            prop->flags        = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC;
            prop->count_values = 2;
            drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "ACTIVE");
            if (prop->values_ptr && req_values >= 2) {
                uint64_t *values = (uint64_t *)(uintptr_t)prop->values_ptr;
                values[0]        = 0;
                values[1]        = 1;
            }
            return 0;

        case DRM_CONNECTOR_DPMS_PROP_ID:
            prop->flags = DRM_MODE_PROP_ENUM;
            drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "DPMS");
            prop->count_enum_blobs = 4;
            if (prop->enum_blob_ptr && req_enum_blobs > 0) {
                struct drm_mode_property_enum *enums =
                    (struct drm_mode_property_enum *)(uintptr_t)prop->enum_blob_ptr;
                if (req_enum_blobs > 0) {
                    drm_copy_property_enum(&enums[0], DRM_MODE_DPMS_ON, "On");
                }
                if (req_enum_blobs > 1) {
                    drm_copy_property_enum(&enums[1], DRM_MODE_DPMS_STANDBY, "Standby");
                }
                if (req_enum_blobs > 2) {
                    drm_copy_property_enum(&enums[2], DRM_MODE_DPMS_SUSPEND, "Suspend");
                }
                if (req_enum_blobs > 3) {
                    drm_copy_property_enum(&enums[3], DRM_MODE_DPMS_OFF, "Off");
                }
            }
            return 0;

        case DRM_CONNECTOR_EDID_PROP_ID:
            prop->flags = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_IMMUTABLE;
            drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "EDID");
            return 0;

        case DRM_PROPERTY_ID_IN_FORMATS:
            prop->flags = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_IMMUTABLE;
            drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "IN_FORMATS");
            return 0;

        case DRM_FB_WIDTH_PROP_ID:
        case DRM_FB_HEIGHT_PROP_ID:
        case DRM_FB_BPP_PROP_ID:
        case DRM_FB_DEPTH_PROP_ID:
            prop->flags        = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC;
            prop->count_values = 2;
            if (prop->prop_id == DRM_FB_WIDTH_PROP_ID) {
                drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "WIDTH");
            } else if (prop->prop_id == DRM_FB_HEIGHT_PROP_ID) {
                drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "HEIGHT");
            } else if (prop->prop_id == DRM_FB_BPP_PROP_ID) {
                drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "BPP");
            } else {
                drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "DEPTH");
            }
            if (prop->values_ptr && req_values >= 2) {
                uint64_t *values = (uint64_t *)(uintptr_t)prop->values_ptr;
                if (prop->prop_id == DRM_FB_BPP_PROP_ID || prop->prop_id == DRM_FB_DEPTH_PROP_ID) {
                    values[0] = 8;
                    values[1] = 32;
                } else {
                    values[0] = 1;
                    values[1] = 8192;
                }
            }
            return 0;

        default:
            printk("drm: Unsupported mode property: %#010lx\n", prop->prop_id);
            return -EINVAL;
        }
    }

    case DRM_IOCTL_MODE_GETPROPBLOB: {
        struct drm_mode_get_blob *blob = (struct drm_mode_get_blob *)arg;
        uint32_t req_length          = blob->length;
        uint32_t crtc_id             = 0;
        uint32_t connector_id        = 0;
        uint32_t plane_id            = 0;

        if (blob->blob_id == 0) {
            blob->length = 0;
            return 0;
        }

        if (blob->blob_id == 1 || drm_mode_blob_to_crtc_id(blob->blob_id, &crtc_id)) {
            struct drm_mode_modeinfo mode;
            drm_crtc_t *crtc = NULL;

            if (blob->blob_id == 1 && dev->resource_mgr.crtcs[0]) {
                crtc = dev->resource_mgr.crtcs[0];
            } else if (crtc_id != 0) {
                crtc = drm_crtc_get(&dev->resource_mgr, crtc_id);
            }

            drm_fill_crtc_modeinfo(dev, crtc, &mode);
            blob->length = sizeof(mode);
            if (blob->data) {
                memcpy((void *)(uintptr_t)blob->data, &mode, MIN(req_length, sizeof(mode)));
            }
            if (crtc && crtc != dev->resource_mgr.crtcs[0]) {
                drm_crtc_free(&dev->resource_mgr, crtc->id);
            }
            return 0;
        }

        if (drm_blob_to_connector_edid_id(blob->blob_id, &connector_id)) {
            drm_connector_t *connector = drm_connector_get(&dev->resource_mgr, connector_id);
            uint8_t edid[128];

            if (!connector) {
                return -ENOENT;
            }

            drm_build_connector_edid(dev, connector, edid);
            drm_connector_free(&dev->resource_mgr, connector->id);

            blob->length = sizeof(edid);
            if (blob->data) {
                memcpy((void *)(uintptr_t)blob->data, edid, MIN(req_length, (uint32_t)sizeof(edid)));
            }
            return 0;
        }

        if (drm_blob_to_plane_in_formats_id(blob->blob_id, &plane_id)) {
            drm_plane_t *plane = drm_plane_get(&dev->resource_mgr, plane_id);
            if (!plane) {
                return -ENOENT;
            }

            size_t blob_size = drm_fill_plane_in_formats_blob(plane, NULL, 0);
            uint8_t *plane_blob = malloc(blob_size);
            if (!plane_blob) {
                drm_plane_free(&dev->resource_mgr, plane->id);
                return -ENOMEM;
            }

            drm_fill_plane_in_formats_blob(plane, plane_blob, blob_size);
            drm_plane_free(&dev->resource_mgr, plane->id);

            blob->length = (uint32_t)blob_size;
            if (blob->data) {
                memcpy((void *)(uintptr_t)blob->data, plane_blob, MIN(req_length, (uint32_t)blob_size));
            }
            free(plane_blob);
            return 0;
        }

        spin_lock(drm_user_blobs_lock);
        ssize_t idx = drm_user_blob_find_index_locked(dev, blob->blob_id);
        if (idx >= 0) {
            drm_user_blob_entry_t entry = drm_user_blobs[idx];
            spin_unlock(drm_user_blobs_lock);

            blob->length = entry.length;
            if (blob->data) {
                memcpy((void *)(uintptr_t)blob->data, entry.data, MIN(req_length, entry.length));
            }
            return 0;
        }

        spin_unlock(drm_user_blobs_lock);
        printk("drm: Invalid blob id %d\n", blob->blob_id);
        return -ENOENT;
    }

    case DRM_IOCTL_MODE_CREATEPROPBLOB: {
        struct drm_mode_create_blob *create_blob = (struct drm_mode_create_blob *)arg;
        if (!create_blob->data || create_blob->length == 0
            || create_blob->length > DRM_USER_BLOB_MAX_SIZE) {
            return -EINVAL;
        }

        if (check_user_overflow(create_blob->data, create_blob->length)) {
            return -EFAULT;
        }

        void *blob_data = malloc(create_blob->length);
        if (!blob_data) {
            return -ENOMEM;
        }
        memcpy(blob_data, (void *)(uintptr_t)create_blob->data, create_blob->length);

        int free_slot  = -1;
        uint32_t blob_id = 0;

        spin_lock(drm_user_blobs_lock);
        for (int i = 0; i < DRM_MAX_USER_BLOBS; i++) {
            if (!drm_user_blobs[i].used) {
                free_slot = i;
                break;
            }
        }

        if (free_slot < 0) {
            spin_unlock(drm_user_blobs_lock);
            free(blob_data);
            return -ENOSPC;
        }

        int ret = drm_user_blob_generate_id_locked(&blob_id);
        if (ret != 0) {
            spin_unlock(drm_user_blobs_lock);
            free(blob_data);
            return ret;
        }

        drm_user_blobs[free_slot].used    = true;
        drm_user_blobs[free_slot].dev     = dev;
        drm_user_blobs[free_slot].blob_id = blob_id;
        drm_user_blobs[free_slot].length  = create_blob->length;
        drm_user_blobs[free_slot].data    = blob_data;
        spin_unlock(drm_user_blobs_lock);

        create_blob->blob_id = blob_id;
        return 0;
    }

    case DRM_IOCTL_MODE_DESTROYPROPBLOB: {
        struct drm_mode_destroy_blob *destroy_blob = (struct drm_mode_destroy_blob *)arg;
        if (destroy_blob->blob_id == 0) {
            return -EINVAL;
        }

        void *blob_data = NULL;

        spin_lock(drm_user_blobs_lock);
        ssize_t idx = drm_user_blob_find_index_locked(dev, destroy_blob->blob_id);
        if (idx >= 0) {
            blob_data = drm_user_blobs[idx].data;
            memset(&drm_user_blobs[idx], 0, sizeof(drm_user_blobs[idx]));
            spin_unlock(drm_user_blobs_lock);
            free(blob_data);
            return 0;
        }

        spin_unlock(drm_user_blobs_lock);
        return -ENOENT;
    }

    case DRM_IOCTL_MODE_SETPROPERTY: {
        return 0;
    }

    case DRM_IOCTL_MODE_OBJ_GETPROPERTIES: {
        struct drm_mode_obj_get_properties *props = (struct drm_mode_obj_get_properties *)arg;
        uint32_t req_props                        = props->count_props;
        uint32_t obj_type                         = props->obj_type;

        if (obj_type == DRM_MODE_OBJECT_ANY) {
            int ret = drm_mode_resolve_obj_type(dev, props->obj_id, &obj_type);
            if (ret != 0) {
                return ret;
            }
        }

        switch (obj_type) {
        case DRM_MODE_OBJECT_PLANE: {
            drm_plane_t *plane = NULL;
            for (int idx = 0; idx < DRM_MAX_PLANES_PER_DEVICE; idx++) {
                if (dev->resource_mgr.planes[idx] && dev->resource_mgr.planes[idx]->id == props->obj_id) {
                    plane = dev->resource_mgr.planes[idx];
                    break;
                }
            }
            if (!plane) {
                return -ENOENT;
            }

            props->count_props = 12;
            if (props->props_ptr) {
                uint32_t *prop_ids = (uint32_t *)(uintptr_t)props->props_ptr;
                uint32_t copy_props = MIN(req_props, props->count_props);
                uint32_t ids[12] = {
                    DRM_PROPERTY_ID_PLANE_TYPE,
                    DRM_PROPERTY_ID_IN_FORMATS,
                    DRM_PROPERTY_ID_FB_ID,
                    DRM_PROPERTY_ID_CRTC_ID,
                    DRM_PROPERTY_ID_SRC_X,
                    DRM_PROPERTY_ID_SRC_Y,
                    DRM_PROPERTY_ID_SRC_W,
                    DRM_PROPERTY_ID_SRC_H,
                    DRM_PROPERTY_ID_CRTC_X,
                    DRM_PROPERTY_ID_CRTC_Y,
                    DRM_PROPERTY_ID_CRTC_W,
                    DRM_PROPERTY_ID_CRTC_H,
                };
                memcpy(prop_ids, ids, copy_props * sizeof(uint32_t));
            }
            if (props->prop_values_ptr) {
                uint64_t *prop_values = (uint64_t *)(uintptr_t)props->prop_values_ptr;
                uint32_t copy_props   = MIN(req_props, props->count_props);
                uint64_t values[12]   = {0};
                values[0]             = plane->plane_type;
                values[1]             = drm_plane_in_formats_blob_id(plane->id);
                values[2]             = plane->fb_id;
                values[3]             = plane->crtc_id;

                drm_crtc_t *crtc = plane->crtc_id ? drm_crtc_get(&dev->resource_mgr, plane->crtc_id) : NULL;
                drm_framebuffer_t *fb = plane->fb_id ? drm_framebuffer_get(&dev->resource_mgr, plane->fb_id) : NULL;
                if (fb) {
                    values[6] = ((uint64_t)fb->width) << 16;
                    values[7] = ((uint64_t)fb->height) << 16;
                    drm_framebuffer_free(&dev->resource_mgr, fb->id);
                } else if (crtc) {
                    values[6] = ((uint64_t)crtc->w) << 16;
                    values[7] = ((uint64_t)crtc->h) << 16;
                }
                if (crtc) {
                    values[8]  = crtc->x;
                    values[9]  = crtc->y;
                    values[10] = crtc->w;
                    values[11] = crtc->h;
                    drm_crtc_free(&dev->resource_mgr, crtc->id);
                }

                memcpy(prop_values, values, copy_props * sizeof(uint64_t));
            }
            return 0;
        }

        case DRM_MODE_OBJECT_CRTC: {
            drm_crtc_t *crtc = NULL;
            for (int idx = 0; idx < DRM_MAX_CRTCS_PER_DEVICE; idx++) {
                if (dev->resource_mgr.crtcs[idx] && dev->resource_mgr.crtcs[idx]->id == props->obj_id) {
                    crtc = dev->resource_mgr.crtcs[idx];
                    break;
                }
            }
            if (!crtc) {
                return -ENOENT;
            }

            props->count_props = 2;
            if (props->props_ptr && req_props > 0) {
                uint32_t *prop_ids = (uint32_t *)(uintptr_t)props->props_ptr;
                prop_ids[0]        = DRM_CRTC_ACTIVE_PROP_ID;
                if (req_props > 1) {
                    prop_ids[1] = DRM_CRTC_MODE_ID_PROP_ID;
                }
            }
            if (props->prop_values_ptr && req_props > 0) {
                uint64_t *prop_values = (uint64_t *)(uintptr_t)props->prop_values_ptr;
                prop_values[0]        = 1;
                if (req_props > 1) {
                    prop_values[1] = drm_crtc_mode_blob_id(crtc->id);
                }
            }
            return 0;
        }

        case DRM_MODE_OBJECT_FB: {
            drm_framebuffer_t *fb = NULL;
            for (int idx = 0; idx < DRM_MAX_FRAMEBUFFERS_PER_DEVICE; idx++) {
                if (dev->resource_mgr.framebuffers[idx]
                    && dev->resource_mgr.framebuffers[idx]->id == props->obj_id) {
                    fb = dev->resource_mgr.framebuffers[idx];
                    break;
                }
            }
            if (!fb) {
                return -ENOENT;
            }

            props->count_props = 4;
            if (props->props_ptr && req_props > 0) {
                uint32_t *prop_ids = (uint32_t *)(uintptr_t)props->props_ptr;
                uint32_t copy_props = MIN(req_props, props->count_props);
                uint32_t ids[4] = {
                    DRM_FB_WIDTH_PROP_ID,
                    DRM_FB_HEIGHT_PROP_ID,
                    DRM_FB_BPP_PROP_ID,
                    DRM_FB_DEPTH_PROP_ID,
                };
                memcpy(prop_ids, ids, copy_props * sizeof(uint32_t));
            }
            if (props->prop_values_ptr && req_props > 0) {
                uint64_t *prop_values = (uint64_t *)(uintptr_t)props->prop_values_ptr;
                uint32_t copy_props   = MIN(req_props, props->count_props);
                uint64_t values[4]    = {fb->width, fb->height, fb->bpp, fb->depth};
                memcpy(prop_values, values, copy_props * sizeof(uint64_t));
            }
            return 0;
        }

        case DRM_MODE_OBJECT_CONNECTOR: {
            drm_connector_t *connector = NULL;
            for (int idx = 0; idx < DRM_MAX_CONNECTORS_PER_DEVICE; idx++) {
                if (dev->resource_mgr.connectors[idx]
                    && dev->resource_mgr.connectors[idx]->id == props->obj_id) {
                    connector = dev->resource_mgr.connectors[idx];
                    break;
                }
            }
            if (!connector) {
                return -ENOENT;
            }

            props->count_props = 3;
            if (props->props_ptr && req_props > 0) {
                uint32_t *prop_ids = (uint32_t *)(uintptr_t)props->props_ptr;
                prop_ids[0]        = DRM_CONNECTOR_DPMS_PROP_ID;
                if (req_props > 1) {
                    prop_ids[1] = DRM_CONNECTOR_EDID_PROP_ID;
                }
                if (req_props > 2) {
                    prop_ids[2] = DRM_CONNECTOR_CRTC_ID_PROP_ID;
                }
            }
            if (props->prop_values_ptr && req_props > 0) {
                uint64_t *prop_values = (uint64_t *)(uintptr_t)props->prop_values_ptr;
                prop_values[0]        = DRM_MODE_DPMS_ON;
                if (req_props > 1) {
                    prop_values[1] = drm_connector_edid_blob_id(connector->id);
                }
                if (req_props > 2) {
                    prop_values[2] = connector->crtc_id;
                }
            }
            return 0;
        }

        case DRM_MODE_OBJECT_ENCODER:
            props->count_props = 0;
            return 0;

        default:
            printk("drm: Unsupported mode obj property: %#010lx\n", props->obj_type);
            return -EINVAL;
        }
    }

    case DRM_IOCTL_SET_CLIENT_CAP: {
        struct drm_set_client_cap *cap = (struct drm_set_client_cap *)arg;
        switch (cap->capability) {
        case DRM_CLIENT_CAP_ATOMIC:
            return 0;
        case DRM_CLIENT_CAP_UNIVERSAL_PLANES:
            return 0;
        case DRM_CLIENT_CAP_CURSOR_PLANE_HOTSPOT:
            return 0;
        case DRM_CLIENT_CAP_WRITEBACK_CONNECTORS:
            return 0;
        default:
            printk("drm: Invalid client capability %d\n", cap->capability);
            return -EINVAL;
        }
    }

    case DRM_IOCTL_SET_MASTER: {
        return 0;
    }
    case DRM_IOCTL_DROP_MASTER: {
        return 0;
    }

    case DRM_IOCTL_MODE_GETGAMMA: {
        return 0;
    }
    case DRM_IOCTL_MODE_SETGAMMA: {
        return 0;
    }

    case DRM_IOCTL_MODE_DIRTYFB: {
        return dev->op->dirty_fb ? dev->op->dirty_fb(dev, (struct drm_mode_fb_dirty_cmd *)arg) : 0;
    }

    case DRM_IOCTL_MODE_PAGE_FLIP: {
        return dev->op->page_flip ? dev->op->page_flip(dev, (struct drm_mode_crtc_page_flip *)arg)
                                  : -ENOSYS;
    }

    case DRM_IOCTL_MODE_CURSOR: {
        struct drm_mode_cursor *cmd = (struct drm_mode_cursor *)arg;
        if (cmd->flags & DRM_MODE_CURSOR_BO) {
            return 0;
        } else if (cmd->flags & DRM_MODE_CURSOR_MOVE) {
            return 0;
        }
        break;
    }

    case DRM_IOCTL_MODE_CURSOR2: {
        struct drm_mode_cursor2 *cmd = (struct drm_mode_cursor2 *)arg;
        if (cmd->flags & DRM_MODE_CURSOR_BO) {
            return 0;
        } else if (cmd->flags & DRM_MODE_CURSOR_MOVE) {
            return 0;
        }
        break;
    }

    case DRM_IOCTL_MODE_ATOMIC: {
        return dev->op->atomic_commit ? dev->op->atomic_commit(dev, (struct drm_mode_atomic *)arg)
                                      : -ENOSYS;
    }

    case DRM_IOCTL_WAIT_VBLANK: {
        union drm_wait_vblank *vbl = (union drm_wait_vblank *)arg;

        uint64_t seq = dev->vblank_counter;

        if (vbl->request.type & _DRM_VBLANK_RELATIVE)
            vbl->request.sequence += seq;
        else
            vbl->request.sequence = seq;

        vbl->reply.sequence  = vbl->request.sequence;
        vbl->reply.tval_sec  = nano_time() / 1000000000ULL;
        vbl->reply.tval_usec = (nano_time() % 1000000000ULL) / 1000ULL;

        return 0;
    }

    case DRM_IOCTL_GET_UNIQUE: {
        struct drm_unique *u = (struct drm_unique *)arg;
        char unique[32];

        unique[0] = '\0';
        if (dev->pci_dev) {
            sprintf(
                unique,
                "pci:%04x:%02x:%02x.%u",
                dev->pci_dev->segment,
                dev->pci_dev->bus,
                dev->pci_dev->slot,
                dev->pci_dev->func
            );
        }

        drm_copy_string(u->unique, u->unique_len, unique);
        u->unique_len = strlen(unique);

        return 0;
    }

    case DRM_IOCTL_MODE_LIST_LESSEES: {
        struct drm_mode_list_lessees *l = (struct drm_mode_list_lessees *)arg;

        l->count_lessees = 0;

        return 0;
    }

    case DRM_IOCTL_SET_VERSION: {
        return 0;
    }

    case DRM_IOCTL_GET_MAGIC: {
        drm_auth_t *auth = (drm_auth_t *)arg;
        auth->magic      = 0x12345678;
        return 0;
    }

    case DRM_IOCTL_AUTH_MAGIC: {
        drm_auth_t *auth = (drm_auth_t *)arg;
        if (auth->magic != 0x12345678)
            return -EINVAL;

        return 0;
    }

    default:
        printk("drm: Unsupported ioctl: cmd = %#010lx\n", cmd);
        break;
    }

    return -EINVAL;
}

size_t drm_size_t(void *data) {
    return 0;
}

size_t drm_read(void *data, void *buf, uint64_t offset, uint64_t len) {
    UNUSED(offset);
    drm_device_t *dev = data;
    struct k_drm_event *event = NULL;

    while (!event) {
        spin_lock(dev->event_lock);
        if (dev->drm_events[0]) {
            event = dev->drm_events[0];
            dev->drm_events[0] = NULL;
            memmove(
                &dev->drm_events[0],
                &dev->drm_events[1],
                sizeof(struct k_drm_event *) * (DRM_MAX_EVENTS_COUNT - 1)
            );
            dev->drm_events[DRM_MAX_EVENTS_COUNT - 1] = NULL;
        }
        spin_unlock(dev->event_lock);

        if (!event) {
            scheduler_yield();
        }
    }

    struct drm_event_vblank vbl = {
        .base.type   = event->type,
        .base.length = sizeof(vbl),
        .user_data   = event->user_data,
        .tv_sec      = (uint32_t)event->timestamp.tv_sec,
        .tv_usec     = (uint32_t)(event->timestamp.tv_nsec / 1000ULL),
        .crtc_id     = dev->resource_mgr.crtcs[0] ? dev->resource_mgr.crtcs[0]->id : 0,
    };
    free(event);

    size_t ret = 0;

    if (len >= sizeof(vbl)) {
        memcpy(buf, &vbl, sizeof(vbl));
        ret = sizeof(vbl);
    } else {
        ret = -EINVAL;
    }

    return ret;
}

size_t drm_poll(void *data, size_t event) {
    drm_device_t *dev = (drm_device_t *)data;

    size_t revent = 0;

    if (event & EPOLLIN) {
        spin_lock(dev->event_lock);
        if (dev->drm_events[0]) {
            revent |= EPOLLIN;
        }
        spin_unlock(dev->event_lock);
    }

    return revent;
}

void *drm_map(void *data, void *addr, uint64_t offset, uint64_t len) {
    drm_device_t *dev = (drm_device_t *)data;

    uint64_t page_flags = KERNEL_PTE_FLAGS |
#if defined(__x86_64__) || defined(__amd64__)
                          PTE_USER
#elif defined(__riscv) || defined(__riscv__) || defined(__RISCV_ARCH_RISCV64)
                          ARCH_PT_FLAG_USER
#elif defined(__loongarch__) || defined(__loongarch64)
                          ARCH_PT_FLAG_USER
#endif
        ;

    page_map_range(get_current_directory(), (uint64_t)addr, offset, len, page_flags);

    return addr;
}

drm_device_t *drm_regist_pci_dev(void *data, drm_device_op_t *op, pci_device_t *pci_dev) {
    char buf[64];
    sprintf(buf, "card%d", drm_id);
    drm_device_t *drm_dev = malloc(sizeof(drm_device_t));
    memset(drm_dev, 0, sizeof(drm_device_t));
    drm_dev->id = drm_id + 1;

    // Initialize resource manager
    drm_resource_manager_init(&drm_dev->resource_mgr);
    drm_dev->event_lock = SPIN_INIT;

    drm_dev->data = data;
    drm_dev->op   = op;
    drm_dev->pci_dev = pci_dev;
    drm_device_set_driver_info(drm_dev, DRM_NAME, "20060810", "CoolPotOS DRM");

    // Populate hardware resources if driver supports it
    if (drm_dev->op->get_connectors) {
        drm_connector_t *connectors[DRM_MAX_CONNECTORS_PER_DEVICE];
        memset(connectors, 0, sizeof(connectors));
        uint32_t connector_count = 0;
        if (drm_dev->op->get_connectors(drm_dev, connectors, &connector_count) == 0) {
            for (uint32_t i = 0; i < connector_count && i < DRM_MAX_CONNECTORS_PER_DEVICE; i++) {
                if (connectors[i]) {
                    uint32_t slot = drm_find_free_slot(
                        (void **)drm_dev->resource_mgr.connectors, DRM_MAX_CONNECTORS_PER_DEVICE
                    );
                    if (slot != (uint32_t)-1) {
                        drm_dev->resource_mgr.connectors[slot] = connectors[i];
                        if (connectors[i]->id == 0) {
                            connectors[i]->id = drm_dev->resource_mgr.next_object_id++;
                        }
                        drm_import_resource_id(drm_dev, connectors[i]->id);
                    }
                }
            }
        }
    }

    if (drm_dev->op->get_crtcs) {
        drm_crtc_t *crtcs[DRM_MAX_CRTCS_PER_DEVICE];
        memset(crtcs, 0, sizeof(crtcs));
        uint32_t crtc_count = 0;
        if (drm_dev->op->get_crtcs(drm_dev, crtcs, &crtc_count) == 0) {
            for (uint32_t i = 0; i < crtc_count && i < DRM_MAX_CRTCS_PER_DEVICE; i++) {
                if (crtcs[i]) {
                    uint32_t slot = drm_find_free_slot(
                        (void **)drm_dev->resource_mgr.crtcs, DRM_MAX_CRTCS_PER_DEVICE
                    );
                    if (slot != (uint32_t)-1) {
                        drm_dev->resource_mgr.crtcs[slot] = crtcs[i];
                        if (crtcs[i]->id == 0) {
                            crtcs[i]->id = drm_dev->resource_mgr.next_object_id++;
                        }
                        drm_import_resource_id(drm_dev, crtcs[i]->id);
                    }
                }
            }
        }
    }

    if (drm_dev->op->get_encoders) {
        drm_encoder_t *encoders[DRM_MAX_ENCODERS_PER_DEVICE];
        memset(encoders, 0, sizeof(encoders));
        uint32_t encoder_count = 0;
        if (drm_dev->op->get_encoders(drm_dev, encoders, &encoder_count) == 0) {
            for (uint32_t i = 0; i < encoder_count && i < DRM_MAX_ENCODERS_PER_DEVICE; i++) {
                if (encoders[i]) {
                    uint32_t slot = drm_find_free_slot(
                        (void **)drm_dev->resource_mgr.encoders, DRM_MAX_ENCODERS_PER_DEVICE
                    );
                    if (slot != (uint32_t)-1) {
                        drm_dev->resource_mgr.encoders[slot] = encoders[i];
                        if (encoders[i]->id == 0) {
                            encoders[i]->id = drm_dev->resource_mgr.next_object_id++;
                        }
                        drm_import_resource_id(drm_dev, encoders[i]->id);
                    }
                }
            }
        }
    }

    if (drm_dev->op->get_planes) {
        drm_plane_t *planes[DRM_MAX_PLANES_PER_DEVICE];
        memset(planes, 0, sizeof(planes));
        uint32_t plane_count = 0;
        if (drm_dev->op->get_planes(drm_dev, planes, &plane_count) == 0) {
            for (uint32_t i = 0; i < plane_count && i < DRM_MAX_PLANES_PER_DEVICE; i++) {
                if (planes[i]) {
                    uint32_t slot = drm_find_free_slot(
                        (void **)drm_dev->resource_mgr.planes, DRM_MAX_PLANES_PER_DEVICE
                    );
                    if (slot != (uint32_t)-1) {
                        drm_dev->resource_mgr.planes[slot] = planes[i];
                        if (planes[i]->id == 0) {
                            planes[i]->id = drm_dev->resource_mgr.next_object_id++;
                        }
                        drm_import_resource_id(drm_dev, planes[i]->id);
                    }
                }
            }
        }
    }

    // If no hardware resources were found, create default ones
    if (!drm_dev->resource_mgr.connectors[0]) {
        drm_connector_t *connector =
            drm_connector_alloc(&drm_dev->resource_mgr, DRM_MODE_CONNECTOR_VIRTUAL, NULL);
        if (connector) {
            connector->connection  = DRM_MODE_CONNECTED;
            connector->count_modes = 1;
            connector->modes       = malloc(sizeof(struct drm_mode_modeinfo));
            if (connector->modes) {
                uint32_t width, height, bpp;
                drm_dev->op->get_display_info(drm_dev, &width, &height, &bpp);

                struct drm_mode_modeinfo mode = {
                    .clock       = width * HZ,
                    .hdisplay    = width,
                    .hsync_start = width + 16,
                    .hsync_end   = width + 16 + 96,
                    .htotal      = width + 16 + 96 + 48,
                    .vdisplay    = height,
                    .vsync_start = height + 10,
                    .vsync_end   = height + 10 + 2,
                    .vtotal      = height + 10 + 2 + 33,
                    .vrefresh    = HZ,
                };
                sprintf(mode.name, "%dx%d", width, height);
                memcpy(connector->modes, &mode, sizeof(struct drm_mode_modeinfo));
            }
        }
    }

    if (!drm_dev->resource_mgr.crtcs[0]) {
        drm_crtc_t *crtc = drm_crtc_alloc(&drm_dev->resource_mgr, NULL);
        // CRTC will be configured when used
    }

    if (!drm_dev->resource_mgr.encoders[0]) {
        drm_encoder_t *encoder =
            drm_encoder_alloc(&drm_dev->resource_mgr, DRM_MODE_ENCODER_VIRTUAL, NULL);
        if (encoder && drm_dev->resource_mgr.connectors[0] && drm_dev->resource_mgr.crtcs[0]) {
            encoder->possible_crtcs                         = 1;
            drm_dev->resource_mgr.connectors[0]->encoder_id = encoder->id;
        }
    }

    drm_framebuffer_t *framebuffer = drm_framebuffer_alloc(&drm_dev->resource_mgr, NULL);
    uint32_t width, height, bpp;
    drm_dev->op->get_display_info(drm_dev, &width, &height, &bpp);
    framebuffer->width  = width;
    framebuffer->height = height;
    framebuffer->bpp    = bpp;
    framebuffer->pitch  = width * sizeof(uint32_t);
    framebuffer->depth  = 24;

    char dev_name[32];
    sprintf(dev_name, "card%d", drm_id);

    uint64_t dev_nr = drm_device_install(
        DEV_CHAR, drm_dev, dev_name, 0, drm_ioctl, drm_poll, drm_read, NULL, drm_map
    );
    drm_dev->dev_nr   = dev_nr;
    drm_sysfs_register_device(drm_dev);

    drm_id++;

    return drm_dev;
}
