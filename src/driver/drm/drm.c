#include "driver/drm/drm.h"
#include "driver/drm/drm_core.h"
#include "driver/drm/drm_device.h"
#include "driver/drm/drm_fourcc.h"
#include "driver/pci/pci.h"
#include "errno.h"
#include "fs/sysfs.h"
#include "krlibc.h"
#include "mem/page.h"
#include "task/poll.h"
#include "task/scheduler.h"
#include "term/klog.h"
#include "timer.h"

#define HZ 60

static int drm_id = 0;

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

static void drm_sysfs_write_file(vfs_node_t parent, const char *name, const char *content) {
    vfs_node_t node;
    sysfs_handle_t *handle;
    size_t len;

    if (!parent || !name || !content) {
        return;
    }

    node = sysfs_child_append(parent, name, false);
    if (!node) {
        return;
    }

    handle = node->handle;
    len    = strlen(content);
    handle->data = strdup(content);
    handle->size = len;
    handle->capacity = len + 1;
    node->size = len;
}

static void drm_sysfs_register_device(drm_device_t *drm_dev) {
    vfs_node_t dev_char_dir;
    vfs_node_t dev_root;
    vfs_node_t device_dir;
    vfs_node_t drm_dir;
    vfs_node_t card_dir;
    vfs_node_t class_dir;
    vfs_node_t class_drm_dir;
    char dev_name[32];
    char path[256];
    char content[128];
    int major;
    int minor;

    if (!drm_dev) {
        return;
    }

    dev_char_dir = vfs_open("/sys/dev/char");
    if (!dev_char_dir) {
        return;
    }
    vfs_close(dev_char_dir);

    major = (drm_dev->dev_nr >> 8) & 0xff;
    minor = drm_dev->dev_nr & 0xff;
    sprintf(dev_name, "card%d", minor);

    sprintf(content, "SUBSYSTEM=drm\n");
    dev_root = sysfs_regist_dev('c', major, minor, "", dev_name, content);
    if (!dev_root) {
        return;
    }

    device_dir = sysfs_child_append(dev_root, "device", true);
    if (!device_dir) {
        return;
    }

    drm_dir = sysfs_child_append(device_dir, "drm", true);
    if (!drm_dir) {
        return;
    }

    card_dir = sysfs_child_append(drm_dir, dev_name, true);
    if (!card_dir) {
        return;
    }

    if (drm_dev->pci_dev) {
        sprintf(
            content,
            "PCI_SLOT_NAME=%04x:%02x:%02x.%u\n",
            drm_dev->pci_dev->segment,
            drm_dev->pci_dev->bus,
            drm_dev->pci_dev->slot,
            drm_dev->pci_dev->func
        );
    } else {
        sprintf(content, "PCI_SLOT_NAME=0000:00:00.0\n");
    }
    drm_sysfs_write_file(device_dir, "uevent", content);
    sprintf(content, "0x%04x\n", drm_dev->pci_dev ? drm_dev->pci_dev->vendor_id : 0);
    drm_sysfs_write_file(device_dir, "vendor", content);
    sprintf(content, "0x%04x\n", drm_dev->pci_dev ? drm_dev->pci_dev->device_id : 0);
    drm_sysfs_write_file(device_dir, "device", content);
    sprintf(
        content,
        "0x%04x\n",
        drm_dev->pci_dev ? drm_dev->pci_dev->subsystem_vendor_id : 0
    );
    drm_sysfs_write_file(device_dir, "subsystem_vendor", content);
    sprintf(
        content,
        "0x%04x\n",
        drm_dev->pci_dev ? drm_dev->pci_dev->subsystem_device_id : 0
    );
    drm_sysfs_write_file(device_dir, "subsystem_device", content);
    sprintf(content, "0x%02x\n", drm_dev->pci_dev ? drm_dev->pci_dev->revision_id : 0);
    drm_sysfs_write_file(device_dir, "revision", content);
    drm_sysfs_write_file(drm_dir, "version", "drm 1.1.0 20060810\n");

    sprintf(content, "MAJOR=%d\nMINOR=%d\nDEVNAME=dri/%s\nSUBSYSTEM=drm\n", major, minor, dev_name);
    drm_sysfs_write_file(card_dir, "uevent", content);

    sysfs_child_append_symlink(card_dir, "subsystem", "/sys/class/drm");
    sysfs_child_append_symlink(device_dir, "subsystem", "/sys/bus/pci");

    class_dir = vfs_open("/sys/class");
    if (class_dir) {
        class_drm_dir = vfs_open("/sys/class/drm");
        if (!class_drm_dir) {
            class_drm_dir = sysfs_child_append(class_dir, "drm", true);
        }
        if (class_drm_dir) {
            sprintf(path, "/sys/dev/char/%d:%d/device/drm/%s", major, minor, dev_name);
            sysfs_child_append_symlink(class_drm_dir, dev_name, path);
            vfs_close(class_drm_dir);
        }
        vfs_close(class_dir);
    }
}

void drm_sysfs_populate() {
    const drmd_device_t *device = NULL;

    cow_foreach(drm_devices_get(), device) {
        drm_sysfs_register_device(device->ptr);
    }
}

size_t drm_ioctl(void *data, size_t cmd, size_t arg) {
    drm_device_t *dev = (drm_device_t *)data;
    uint32_t drm_cmd  = cmd & 0xffffffff;

    switch (drm_cmd) {
    case DRM_IOCTL_VERSION: {
        struct drm_version *version = (struct drm_version *)arg;
        static const char driver_name[] = "plainfb";
        static const char driver_date[] = "20260315";
        static const char driver_desc[] = "CoolPotOS DRM/KMS";

        version->version_major      = 2;
        version->version_minor      = 2;
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
            cap->value = 1; // 支持dumb buffer
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
        // case DRM_CAP_CRTC_IN_VBLANK_EVENT:
        //     cap->value = 1;
        //     return 0;
        // case DRM_CAP_PRIME:
        //     cap->value = DRM_PRIME_CAP_IMPORT | DRM_PRIME_CAP_EXPORT;
        //     return 0;
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

        uint32_t width, height, bpp;
        dev->op->get_display_info(dev, &width, &height, &bpp);

        struct drm_mode_modeinfo mode;
        drm_fill_display_mode(dev, &mode, width, height);

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
        return 0;
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
        conn->count_props    = connector->count_props;
        conn->count_encoders = 1; // For now, assume 1 encoder per connector

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
        if (encoders && req_encoders > 0) {
            encoders[0] = connector->encoder_id;
        }

        // Fill properties if pointers provided
        if (conn->props_ptr && conn->prop_values_ptr && req_props > 0) {
            uint32_t *prop_ids    = (uint32_t *)(uintptr_t)conn->props_ptr;
            uint64_t *prop_values = (uint64_t *)(uintptr_t)conn->prop_values_ptr;
            for (uint32_t i = 0; i < MIN(req_props, connector->count_props); i++) {
                prop_ids[i]    = connector->prop_ids[i];
                prop_values[i] = connector->prop_values[i];
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
    case DRM_IOCTL_MODE_SETCRTC: {
        struct drm_mode_crtc *crtc_cmd = (struct drm_mode_crtc *)arg;

        // Find the CRTC by ID
        drm_crtc_t *crtc = drm_crtc_get(&dev->resource_mgr, crtc_cmd->crtc_id);
        if (!crtc) {
            return -ENOENT;
        }

        // Update CRTC state
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
        case DRM_PROPERTY_ID_PLANE_TYPE:
            prop->flags = DRM_MODE_PROP_ENUM;
            drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "type");
            prop->count_enum_blobs = 1;

            if (prop->enum_blob_ptr && req_enum_blobs > 0) {
                struct drm_mode_property_enum *enums =
                    (struct drm_mode_property_enum *)prop->enum_blob_ptr;
                drm_copy_property_enum(&enums[0], DRM_PLANE_TYPE_PRIMARY, "Primary");
            }
            return 0;

        case DRM_CRTC_MODE_ID_PROP_ID:
            prop->flags = DRM_MODE_PROP_BLOB;
            drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "MODE_ID");
            return 0;

        case DRM_CRTC_ACTIVE_PROP_ID:
            prop->flags        = DRM_MODE_PROP_RANGE;
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
            prop->flags = DRM_MODE_PROP_BLOB;
            drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "EDID");
            return 0;

        case DRM_CONNECTOR_CRTC_ID_PROP_ID:
            prop->flags        = DRM_MODE_PROP_RANGE;
            prop->count_values = 2;
            drm_copy_string((char *)prop->name, DRM_PROP_NAME_LEN, "CRTC_ID");
            if (prop->values_ptr && req_values >= 2) {
                uint64_t *values = (uint64_t *)(uintptr_t)prop->values_ptr;
                uint64_t max_id  = dev->resource_mgr.crtcs[0] ? dev->resource_mgr.crtcs[0]->id : 0;
                values[0]        = 0;
                values[1]        = max_id;
            }
            return 0;

        default:
            printk("drm: Unsupported mode property: %#010lx\n", prop->prop_id);
            return -EINVAL;
        }
    }

    case DRM_IOCTL_MODE_GETPROPBLOB: {
        struct drm_mode_get_blob *blob = (struct drm_mode_get_blob *)arg;
        struct drm_mode_modeinfo mode;
        uint32_t width, height, bpp;
        uint32_t req_length = blob->length;

        switch (blob->blob_id) {
        case 0:
            blob->length = 0;
            return 0;
        case 1:
            dev->op->get_display_info(dev, &width, &height, &bpp);
            drm_fill_display_mode(dev, &mode, width, height);
            blob->length = sizeof(mode);
            if (blob->data) {
                memcpy((void *)(uintptr_t)blob->data, &mode, MIN(req_length, sizeof(mode)));
            }
            return 0;

        default:
            printk("drm: Invalid blob id %d\n", blob->blob_id);
            return -ENOENT;
        }
    }

    case DRM_IOCTL_MODE_SETPROPERTY: {
        return 0;
    }

    case DRM_IOCTL_MODE_OBJ_GETPROPERTIES: {
        struct drm_mode_obj_get_properties *props = (struct drm_mode_obj_get_properties *)arg;
        uint32_t req_props                        = props->count_props;

        switch (props->obj_type) {
        case DRM_MODE_OBJECT_ANY:;
            int i = 0;
            for (int idx = 0; idx < DRM_MAX_FRAMEBUFFERS_PER_DEVICE; idx++) {
                if (dev->resource_mgr.framebuffers[idx]) {
                    i++;
                }
            }
            for (int idx = 0; idx < DRM_MAX_PLANES_PER_DEVICE; idx++) {
                if (dev->resource_mgr.planes[idx]) {
                    i++;
                }
            }
            for (int idx = 0; idx < DRM_MAX_CRTCS_PER_DEVICE; idx++) {
                if (dev->resource_mgr.crtcs[idx]) {
                    i++;
                }
            }

            props->count_props = i;

            i = 0;
            if (props->props_ptr && req_props > 0) {
                uint32_t *prop_ids = (uint32_t *)(uintptr_t)props->props_ptr;
                for (int idx = 0; idx < DRM_MAX_FRAMEBUFFERS_PER_DEVICE; idx++) {
                    if (dev->resource_mgr.framebuffers[idx] && i < (int)req_props) {
                        prop_ids[i++] = DRM_PROPERTY_ID_FB_ID;
                    }
                }
                for (int idx = 0; idx < DRM_MAX_PLANES_PER_DEVICE; idx++) {
                    if (dev->resource_mgr.planes[idx] && i < (int)req_props) {
                        prop_ids[i++] = DRM_PROPERTY_ID_PLANE_TYPE;
                    }
                }
                for (int idx = 0; idx < DRM_MAX_CRTCS_PER_DEVICE; idx++) {
                    if (dev->resource_mgr.crtcs[idx] && i < (int)req_props) {
                        prop_ids[i++] = DRM_PROPERTY_ID_CRTC_ID;
                    }
                }
            }

            i = 0;
            if (props->prop_values_ptr && req_props > 0) {
                uint64_t *prop_values = (uint64_t *)(uintptr_t)props->prop_values_ptr;
                for (int idx = 0; idx < DRM_MAX_FRAMEBUFFERS_PER_DEVICE; idx++) {
                    if (dev->resource_mgr.framebuffers[idx] && i < (int)req_props) {
                        prop_values[i++] = dev->resource_mgr.framebuffers[idx]->id;
                    }
                }
                for (int idx = 0; idx < DRM_MAX_PLANES_PER_DEVICE; idx++) {
                    if (dev->resource_mgr.planes[idx] && i < (int)req_props) {
                        prop_values[i++] = dev->resource_mgr.planes[idx]->plane_type;
                    }
                }
                for (int idx = 0; idx < DRM_MAX_CRTCS_PER_DEVICE; idx++) {
                    if (dev->resource_mgr.crtcs[idx] && i < (int)req_props) {
                        prop_values[i++] = dev->resource_mgr.crtcs[idx]->id;
                    }
                }
            }
            break;

        case DRM_MODE_OBJECT_PLANE:
            props->count_props = 1;
            if (props->props_ptr && req_props > 0) {
                uint32_t *prop_ids = (uint32_t *)(uintptr_t)props->props_ptr;
                prop_ids[0]        = DRM_PROPERTY_ID_PLANE_TYPE;
            }
            if (props->prop_values_ptr && req_props > 0) {
                uint64_t *prop_values = (uint64_t *)(uintptr_t)props->prop_values_ptr;
                prop_values[0]        = DRM_PLANE_TYPE_PRIMARY;
            }
            break;

        case DRM_MODE_OBJECT_CRTC:
            props->count_props = 2;
            if (props->props_ptr && req_props > 0) {
                uint32_t *prop_ids = (uint32_t *)(uintptr_t)props->props_ptr;
                prop_ids[0]        = DRM_CRTC_ACTIVE_PROP_ID;  // 激活状态属性
                if (req_props > 1) {
                    prop_ids[1] = DRM_CRTC_MODE_ID_PROP_ID; // 当前模式ID
                }
            }
            if (props->prop_values_ptr && req_props > 0) {
                uint64_t *prop_values = (uint64_t *)(uintptr_t)props->prop_values_ptr;
                prop_values[0]        = 1; // 假设CRTC始终处于激活状态
                if (req_props > 1) {
                    prop_values[1] = 1; // 当前模式ID=1
                }
            }
            break;

        case DRM_MODE_OBJECT_CONNECTOR:
            props->count_props = 3;
            if (props->props_ptr && req_props > 0) {
                uint32_t *prop_ids = (uint32_t *)(uintptr_t)props->props_ptr;
                prop_ids[0]        = DRM_CONNECTOR_DPMS_PROP_ID;    // DPMS状态
                if (req_props > 1) {
                    prop_ids[1] = DRM_CONNECTOR_EDID_PROP_ID; // EDID信息
                }
                if (req_props > 2) {
                    prop_ids[2] = DRM_CONNECTOR_CRTC_ID_PROP_ID; // 关联的CRTC
                }
            }
            if (props->prop_values_ptr && req_props > 0) {
                uint64_t *prop_values = (uint64_t *)(uintptr_t)props->prop_values_ptr;
                prop_values[0]        = DRM_MODE_DPMS_ON; // 电源开启状态
                if (req_props > 1) {
                    prop_values[1] = 0; // EDID句柄(暂未提供)
                }
                if (req_props > 2) {
                    prop_values[2] = dev->resource_mgr.crtcs[0] ? dev->resource_mgr.crtcs[0]->id : 0;
                }
            }
            break;

        default:
            printk("drm: Unsupported mode obj property: %#010lx\n", props->obj_type);
            return -EINVAL;
        }

        return 0;
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
        return 0;
    }

    case DRM_IOCTL_MODE_PAGE_FLIP: {
        return dev->op->page_flip(dev, (struct drm_mode_crtc_page_flip *)arg);
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
        static const char unique[] = "";

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
    drm_device_t *dev = data;

    while (!dev->drm_events[0]) {
        scheduler_yield();
    }

    struct drm_event_vblank vbl = {
        .base.type   = dev->drm_events[0]->type,
        .base.length = sizeof(vbl),
        .user_data   = dev->drm_events[0]->user_data,
        .tv_sec      = nano_time() / 1000000000,
        .tv_usec     = (nano_time() % 1000000000) / 1000,
        .crtc_id     = dev->resource_mgr.crtcs[0]->id,
    };

    free(dev->drm_events[0]);

    dev->drm_events[0] = NULL;

    memmove(
        &dev->drm_events[0],
        &dev->drm_events[1],
        sizeof(struct k_drm_event *) * (DRM_MAX_EVENTS_COUNT - 1)
    );

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
        if (dev->drm_events[0]) {
            revent |= EPOLLIN;
        }
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

    drm_dev->data = data;
    drm_dev->op   = op;

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
                        connectors[i]->id = drm_dev->resource_mgr.next_connector_id++;
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
                        crtcs[i]->id                      = drm_dev->resource_mgr.next_crtc_id++;
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
                        encoders[i]->id = drm_dev->resource_mgr.next_encoder_id++;
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
                        planes[i]->id                      = drm_dev->resource_mgr.next_plane_id++;
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
    drm_dev->pci_dev  = pci_dev;
    drm_sysfs_register_device(drm_dev);

    drm_id++;

    return drm_dev;
}
