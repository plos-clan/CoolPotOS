#pragma once

#include "driver/drm/drm_core.h"
#include "driver/drm/drm.h"
#include "driver/drm/drm_fourcc.h"
#include "boot.h"

typedef struct plainfb_device {
    boot_framebuffer_t *framebuffer;

    // Framebuffer management
    struct plainfb_dumbbuffer {
        bool used;
        uint64_t addr;
        uint32_t width;
        uint32_t height;
        uint32_t pitch;
        int refcount;
    } dumbbuffers[32];

    // DRM resources
    drm_connector_t *connectors[16];
    drm_crtc_t *crtcs[16];
    drm_encoder_t *encoders[16];
    drm_resource_manager_t resource_mgr;
} plainfb_device_t;

void drm_plainfb_init();
