#pragma once

#define DRM_MAX_DEVICES 8
#define DRM_MAX_CONNECTORS_PER_DEVICE 4
#define DRM_MAX_CRTCS_PER_DEVICE 2
#define DRM_MAX_ENCODERS_PER_DEVICE 2
#define DRM_MAX_FRAMEBUFFERS_PER_DEVICE 16
#define DRM_MAX_PLANES_PER_DEVICE 4

#include "drm_mode.h"
#include "lock.h"
#include "types.h"

typedef enum {
    DRM_RESOURCE_CONNECTOR,
    DRM_RESOURCE_CRTC,
    DRM_RESOURCE_ENCODER,
    DRM_RESOURCE_FRAMEBUFFER,
    DRM_RESOURCE_PLANE
} drm_resource_type_t;

typedef struct drm_connector {
    uint32_t id;
    uint32_t type;
    uint32_t connection;
    uint32_t encoder_id;
    uint32_t crtc_id;
    uint32_t mm_width;
    uint32_t mm_height;
    uint32_t subpixel;
    uint32_t count_modes;
    struct drm_mode_modeinfo *modes;
    uint32_t count_props;
    uint32_t *prop_ids;
    uint64_t *prop_values;
    void *driver_data;
    uint32_t refcount;
} drm_connector_t;

typedef struct drm_crtc {
    uint32_t id;
    uint32_t fb_id;
    uint32_t x;
    uint32_t y;
    uint32_t gamma_size;
    uint32_t mode_valid;
    struct drm_mode_modeinfo mode;
    uint32_t *prop_ids;
    uint64_t *prop_values;
    uint32_t count_props;
    void *driver_data;
    uint32_t refcount;
} drm_crtc_t;

typedef struct drm_encoder {
    uint32_t id;
    uint32_t type;
    uint32_t crtc_id;
    uint32_t possible_crtcs;
    uint32_t possible_clones;
    void *driver_data;
    uint32_t refcount;
} drm_encoder_t;

typedef struct drm_framebuffer {
    uint32_t id;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t handle;
    uint64_t modifier;
    uint32_t format;
    uint32_t flags;
    void *driver_data;
    uint32_t refcount;
} drm_framebuffer_t;

enum drm_plane_type {
    /**
     * @DRM_PLANE_TYPE_OVERLAY:
     *
     * Overlay planes represent all non-primary, non-cursor planes. Some
     * drivers refer to these types of planes as "sprites" internally.
     */
    DRM_PLANE_TYPE_OVERLAY,

    /**
     * @DRM_PLANE_TYPE_PRIMARY:
     *
     * A primary plane attached to a CRTC is the most likely to be able to
     * light up the CRTC when no scaling/cropping is used and the plane
     * covers the whole CRTC.
     */
    DRM_PLANE_TYPE_PRIMARY,

    /**
     * @DRM_PLANE_TYPE_CURSOR:
     *
     * A cursor plane attached to a CRTC is more likely to be able to be
     * enabled when no scaling/cropping is used and the framebuffer has the
     * size indicated by &drm_mode_config.cursor_width and
     * &drm_mode_config.cursor_height. Additionally, if the driver doesn't
     * support modifiers, the framebuffer should have a linear layout.
     */
    DRM_PLANE_TYPE_CURSOR,
};

typedef struct drm_plane {
    uint32_t id;
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t possible_crtcs;
    uint32_t gamma_size;
    uint32_t count_format_types;
    uint32_t *format_types;
    void *driver_data;
    uint32_t refcount;
    enum drm_plane_type plane_type;
} drm_plane_t;

typedef struct drm_resource_manager {
    drm_connector_t *connectors[DRM_MAX_CONNECTORS_PER_DEVICE];
    drm_crtc_t *crtcs[DRM_MAX_CRTCS_PER_DEVICE];
    drm_encoder_t *encoders[DRM_MAX_ENCODERS_PER_DEVICE];
    drm_framebuffer_t *framebuffers[DRM_MAX_FRAMEBUFFERS_PER_DEVICE];
    drm_plane_t *planes[DRM_MAX_PLANES_PER_DEVICE];

    uint32_t next_connector_id;
    uint32_t next_crtc_id;
    uint32_t next_encoder_id;
    uint32_t next_framebuffer_id;
    uint32_t next_plane_id;

    spin_t lock;
} drm_resource_manager_t;

// Resource management functions
drm_connector_t *drm_connector_alloc(drm_resource_manager_t *mgr, uint32_t type, void *driver_data);
void drm_connector_free(drm_resource_manager_t *mgr, uint32_t id);
drm_connector_t *drm_connector_get(drm_resource_manager_t *mgr, uint32_t id);

drm_crtc_t *drm_crtc_alloc(drm_resource_manager_t *mgr, void *driver_data);
void drm_crtc_free(drm_resource_manager_t *mgr, uint32_t id);
drm_crtc_t *drm_crtc_get(drm_resource_manager_t *mgr, uint32_t id);

drm_encoder_t *drm_encoder_alloc(drm_resource_manager_t *mgr, uint32_t type, void *driver_data);
void drm_encoder_free(drm_resource_manager_t *mgr, uint32_t id);
drm_encoder_t *drm_encoder_get(drm_resource_manager_t *mgr, uint32_t id);

drm_framebuffer_t *drm_framebuffer_alloc(drm_resource_manager_t *mgr, void *driver_data);
void drm_framebuffer_free(drm_resource_manager_t *mgr, uint32_t id);
drm_framebuffer_t *drm_framebuffer_get(drm_resource_manager_t *mgr, uint32_t id);

drm_plane_t *drm_plane_alloc(drm_resource_manager_t *mgr, void *driver_data);
void drm_plane_free(drm_resource_manager_t *mgr, uint32_t id);
drm_plane_t *drm_plane_get(drm_resource_manager_t *mgr, uint32_t id);

// Utility functions
uint32_t drm_find_free_slot(void **array, uint32_t size);
void drm_resource_manager_init(drm_resource_manager_t *mgr);
void drm_resource_manager_cleanup(drm_resource_manager_t *mgr);
