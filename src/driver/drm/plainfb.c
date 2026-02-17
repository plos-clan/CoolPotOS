#include "driver/drm/plainfb.h"
#include "driver/pci/pci.h"
#include "errno.h"
#include "krlibc.h"
#include "mem/frame.h"
#include "mem/heap.h"
#include "mem/page.h"

int plainfb_get_display_info(
    drm_device_t *drm_dev, uint32_t *width, uint32_t *height, uint32_t *bpp
) {
    plainfb_device_t *dev = drm_dev->data;
    *width                = dev->framebuffer->width;
    *height               = dev->framebuffer->height;
    *bpp                  = dev->framebuffer->bpp;
    return 0;
}

int plainfb_create_dumb(drm_device_t *drm_dev, struct drm_mode_create_dumb *args) {
    plainfb_device_t *gpu_dev = drm_dev->data;

    args->pitch = args->width * (args->bpp / 8);
    args->size  = args->height * args->pitch;

    // Find free framebuffer slot
    for (uint32_t i = 0; i < 32; i++) {
        if (!gpu_dev->dumbbuffers[i].used) {
            gpu_dev->dumbbuffers[i].used     = true;
            gpu_dev->dumbbuffers[i].width    = args->width;
            gpu_dev->dumbbuffers[i].height   = args->height;
            gpu_dev->dumbbuffers[i].pitch    = args->pitch;
            gpu_dev->dumbbuffers[i].refcount = 1;

            // Allocate memory for framebuffer
            gpu_dev->dumbbuffers[i].addr = alloc_frames((args->size + PAGE_SIZE - 1) / PAGE_SIZE);

            args->handle = i;
            return 0;
        }
    }

    return -ENOSPC;
}

static int plainfb_destroy_dumb(drm_device_t *drm_dev, uint32_t handle) {
    plainfb_device_t *gpu_dev = drm_dev->data;

    if (handle >= 32 || !gpu_dev->dumbbuffers[handle].used) {
        return -EINVAL;
    }

    if (--gpu_dev->dumbbuffers[handle].refcount == 0) {
        // Free memory
        free_frames(
            gpu_dev->dumbbuffers[handle].addr,
            (gpu_dev->dumbbuffers[handle].pitch * gpu_dev->dumbbuffers[handle].height + PAGE_SIZE
             - 1)
                / PAGE_SIZE
        );

        gpu_dev->dumbbuffers[handle].used = false;
    }

    return 0;
}

static int plainfb_add_fb(drm_device_t *drm_dev, struct drm_mode_fb_cmd *fb_cmd) {
    plainfb_device_t *device = drm_dev->data;

    drm_framebuffer_t *fb = drm_framebuffer_alloc(&device->resource_mgr, device);

    fb->width  = fb_cmd->width;
    fb->height = fb_cmd->height;
    fb->pitch  = fb_cmd->pitch;
    fb->bpp    = fb_cmd->bpp;
    fb->depth  = fb_cmd->depth;
    fb->handle = fb_cmd->handle;
    fb->format = DRM_FORMAT_BGRA8888;

    fb_cmd->fb_id = fb->id;

    return 0;
}

static int plainfb_add_fb2(drm_device_t *drm_dev, struct drm_mode_fb_cmd2 *fb_cmd) {
    plainfb_device_t *device = drm_dev->data;

    drm_framebuffer_t *fb = drm_framebuffer_alloc(&device->resource_mgr, device);
    if (!fb) {
        return -ENOMEM;
    }

    fb->width    = fb_cmd->width;
    fb->height   = fb_cmd->height;
    fb->pitch    = fb_cmd->pitches[0];
    fb->bpp      = 32;
    fb->depth    = 24;
    fb->handle   = fb_cmd->handles[0];
    fb->format   = fb_cmd->pixel_format;
    fb->modifier = fb_cmd->modifier[0];

    fb_cmd->fb_id = fb->id;

    return 0;
}

int plainfb_map_dumb(drm_device_t *drm_dev, struct drm_mode_map_dumb *args) {
    plainfb_device_t *gpu_dev = drm_dev->data;

    if (args->handle >= 32 || !gpu_dev->dumbbuffers[args->handle].used) {
        return -EINVAL;
    }

    args->offset = gpu_dev->dumbbuffers[args->handle].addr;

    return 0;
}

static int plainfb_set_crtc(drm_device_t *drm_dev, struct drm_mode_crtc *crtc) {
    // CRTC configuration handled by page flip
    return 0;
}

static int plainfb_page_flip(drm_device_t *drm_dev, struct drm_mode_crtc_page_flip *flip) {
    plainfb_device_t *gpu_dev = drm_dev->data;

    if (flip->crtc_id > 1 || flip->fb_id >= 32 || !gpu_dev->dumbbuffers[flip->fb_id - 1].used) {
        return -EINVAL;
    }

    memcpy(
        (void *)gpu_dev->framebuffer->address,
        (const void *)phys_to_virt(gpu_dev->dumbbuffers[flip->fb_id - 1].addr),
        gpu_dev->dumbbuffers[flip->fb_id - 1].pitch * gpu_dev->dumbbuffers[flip->fb_id - 1].height
    );

    // Create flip complete event
    for (int i = 0; i < DRM_MAX_EVENTS_COUNT; i++) {
        if (!drm_dev->drm_events[i]) {
            drm_dev->drm_events[i]                    = malloc(sizeof(struct k_drm_event));
            drm_dev->drm_events[i]->type              = DRM_EVENT_FLIP_COMPLETE;
            drm_dev->drm_events[i]->user_data         = flip->user_data;
            drm_dev->drm_events[i]->timestamp.tv_sec  = nano_time() / 1000000000ULL;
            drm_dev->drm_events[i]->timestamp.tv_nsec = nano_time() % 1000000000ULL;
            break;
        }
    }

    return 0;
}

static int
plainfb_get_connectors(drm_device_t *drm_dev, drm_connector_t **connectors, uint32_t *count) {
    plainfb_device_t *gpu_dev = drm_dev->data;
    *count                    = 0;

    for (uint32_t i = 0; i < 1; i++) {
        if (gpu_dev->connectors[i]) {
            connectors[(*count)++] = gpu_dev->connectors[i];
        }
    }

    return 0;
}

static int plainfb_get_crtcs(drm_device_t *drm_dev, drm_crtc_t **crtcs, uint32_t *count) {
    plainfb_device_t *gpu_dev = drm_dev->data;
    *count                    = 0;

    for (uint32_t i = 0; i < 1; i++) {
        if (gpu_dev->crtcs[i]) {
            crtcs[(*count)++] = gpu_dev->crtcs[i];
        }
    }

    return 0;
}

static int plainfb_get_encoders(drm_device_t *drm_dev, drm_encoder_t **encoders, uint32_t *count) {
    plainfb_device_t *gpu_dev = drm_dev->data;
    *count                    = 0;

    for (uint32_t i = 0; i < 1; i++) {
        if (gpu_dev->encoders[i]) {
            encoders[(*count)++] = gpu_dev->encoders[i];
        }
    }

    return 0;
}

int plainfb_get_planes(drm_device_t *drm_dev, drm_plane_t **planes, uint32_t *count) {
    plainfb_device_t *device = drm_dev->data;

    *count                        = 1;
    planes[0]                     = drm_plane_alloc(&device->resource_mgr, drm_dev->data);
    planes[0]->crtc_id            = device->crtcs[0]->id;
    planes[0]->fb_id              = device->crtcs[0]->fb_id;
    planes[0]->possible_crtcs     = 1;
    planes[0]->count_format_types = 1;
    planes[0]->format_types       = malloc(sizeof(uint32_t) * planes[0]->count_format_types);
    planes[0]->format_types[0]    = DRM_FORMAT_BGRA8888;
    planes[0]->plane_type         = DRM_PLANE_TYPE_PRIMARY;
    return 0;
}

// DRM device operations structure
drm_device_op_t plainfb_drm_device_op = {
    .get_display_info = plainfb_get_display_info,
    .get_fb           = NULL,
    .create_dumb      = plainfb_create_dumb,
    .destroy_dumb     = plainfb_destroy_dumb,
    .dirty_fb         = NULL,
    .add_fb           = plainfb_add_fb,
    .add_fb2          = plainfb_add_fb2,
    .set_plane        = NULL,
    .atomic_commit    = NULL,
    .map_dumb         = plainfb_map_dumb,
    .set_crtc         = plainfb_set_crtc,
    .page_flip        = plainfb_page_flip,
    .set_cursor       = NULL,
    .gamma_set        = NULL,
    .get_connectors   = plainfb_get_connectors,
    .get_crtcs        = plainfb_get_crtcs,
    .get_encoders     = plainfb_get_encoders,
    .get_planes       = plainfb_get_planes,
};

pci_device_t *vga_pci_devices[8];
uint32_t count;

void drm_load_device(pci_device_t *device) {
    vga_pci_devices[count++] = device;
}

void drm_plainfb_init() {
    boot_framebuffer_t *fb = boot_get_framebuffer(0);

    // Create GPU device structure
    plainfb_device_t *gpu_device = malloc(sizeof(plainfb_device_t));
    memset(gpu_device, 0, sizeof(plainfb_device_t));

    gpu_device->framebuffer = fb;

    // Initialize DRM resource manager
    drm_resource_manager_init(&gpu_device->resource_mgr);

    int i = 0;
    // Create connector
    gpu_device->connectors[i] =
        drm_connector_alloc(&gpu_device->resource_mgr, DRM_MODE_CONNECTOR_VIRTUAL, gpu_device);
    if (gpu_device->connectors[i]) {
        gpu_device->connectors[i]->connection = DRM_MODE_CONNECTED;
        gpu_device->connectors[i]->mm_width   = fb->width;
        gpu_device->connectors[i]->mm_height  = fb->height;

        // Add display mode
        gpu_device->connectors[i]->modes = malloc(sizeof(struct drm_mode_modeinfo));
        struct drm_mode_modeinfo mode    = {
               .clock       = fb->width * 60,
               .hdisplay    = fb->width,
               .hsync_start = fb->width + 16,
               .hsync_end   = fb->width + 16 + 96,
               .htotal      = fb->width + 16 + 96 + 48,
               .vdisplay    = fb->height,
               .vsync_start = fb->height + 10,
               .vsync_end   = fb->height + 10 + 2,
               .vtotal      = fb->height + 10 + 2 + 33,
               .vrefresh    = 60,
        };
        memcpy(gpu_device->connectors[i]->modes, &mode, sizeof(struct drm_mode_modeinfo));
        gpu_device->connectors[i]->count_modes = 1;
    }

    // Create CRTC
    gpu_device->crtcs[i] = drm_crtc_alloc(&gpu_device->resource_mgr, gpu_device);

    // Create encoder
    gpu_device->encoders[i] =
        drm_encoder_alloc(&gpu_device->resource_mgr, DRM_MODE_ENCODER_VIRTUAL, gpu_device);

    if (gpu_device->encoders[i] && gpu_device->connectors[i] && gpu_device->crtcs[i]) {
        gpu_device->encoders[i]->possible_crtcs = 1 << i;
        gpu_device->connectors[i]->encoder_id   = gpu_device->encoders[i]->id;
        gpu_device->connectors[i]->crtc_id      = gpu_device->crtcs[i]->id;
    }

    memset(gpu_device->dumbbuffers, 0, sizeof(gpu_device->dumbbuffers));

    pci_find_class(0x00020000, drm_load_device);

    if (count > 0) {
        // Register with DRM subsystem
        drm_regist_pci_dev(gpu_device, &plainfb_drm_device_op, vga_pci_devices[0]);
    }
}
