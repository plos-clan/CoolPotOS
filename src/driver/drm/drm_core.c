#include "driver/drm/drm_core.h"
#include "driver/drm/drm.h"
#include "krlibc.h"
#include "mem/heap.h"

// Utility function to find a free slot in an array
uint32_t drm_find_free_slot(void **array, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        if (array[i] == NULL) {
            return i;
        }
    }
    return (uint32_t)-1;
}

// Resource manager initialization and cleanup
void drm_resource_manager_init(drm_resource_manager_t *mgr) {
    memset(mgr, 0, sizeof(drm_resource_manager_t));
    mgr->lock = SPIN_INIT;
    mgr->next_connector_id = 1;
    mgr->next_crtc_id = 1;
    mgr->next_encoder_id = 1;
    mgr->next_framebuffer_id = 1;
    mgr->next_plane_id = 1;
}

void drm_resource_manager_cleanup(drm_resource_manager_t *mgr) {
    spin_lock(mgr->lock);

    // Free all connectors
    for (uint32_t i = 0; i < DRM_MAX_CONNECTORS_PER_DEVICE; i++) {
        if (mgr->connectors[i]) {
            free(mgr->connectors[i]);
            mgr->connectors[i] = NULL;
        }
    }

    // Free all CRTCs
    for (uint32_t i = 0; i < DRM_MAX_CRTCS_PER_DEVICE; i++) {
        if (mgr->crtcs[i]) {
            free(mgr->crtcs[i]);
            mgr->crtcs[i] = NULL;
        }
    }

    // Free all encoders
    for (uint32_t i = 0; i < DRM_MAX_ENCODERS_PER_DEVICE; i++) {
        if (mgr->encoders[i]) {
            free(mgr->encoders[i]);
            mgr->encoders[i] = NULL;
        }
    }

    // Free all framebuffers
    for (uint32_t i = 0; i < DRM_MAX_FRAMEBUFFERS_PER_DEVICE; i++) {
        if (mgr->framebuffers[i]) {
            free(mgr->framebuffers[i]);
            mgr->framebuffers[i] = NULL;
        }
    }

    // Free all planes
    for (uint32_t i = 0; i < DRM_MAX_PLANES_PER_DEVICE; i++) {
        if (mgr->planes[i]) {
            free(mgr->planes[i]);
            mgr->planes[i] = NULL;
        }
    }

    spin_unlock(mgr->lock);
}

// Connector management
drm_connector_t *
drm_connector_alloc(drm_resource_manager_t *mgr, uint32_t type, void *driver_data) {
    spin_lock(mgr->lock);

    uint32_t slot = drm_find_free_slot((void **)mgr->connectors, DRM_MAX_CONNECTORS_PER_DEVICE);
    if (slot == (uint32_t)-1) {
        spin_unlock(mgr->lock);
        return NULL;
    }

    drm_connector_t *connector = malloc(sizeof(drm_connector_t));
    if (!connector) {
        spin_unlock(mgr->lock);
        return NULL;
    }

    memset(connector, 0, sizeof(drm_connector_t));
    connector->id = mgr->next_connector_id++;
    connector->type = type;
    connector->connection = DRM_MODE_CONNECTED;
    connector->driver_data = driver_data;
    connector->refcount = 1;

    mgr->connectors[slot] = connector;
    spin_unlock(mgr->lock);

    return connector;
}

void drm_connector_free(drm_resource_manager_t *mgr, uint32_t id) {
    spin_lock(mgr->lock);

    for (uint32_t i = 0; i < DRM_MAX_CONNECTORS_PER_DEVICE; i++) {
        if (mgr->connectors[i] && mgr->connectors[i]->id == id) {
            if (--mgr->connectors[i]->refcount == 0) {
                free(mgr->connectors[i]);
                mgr->connectors[i] = NULL;
            }
            break;
        }
    }

    spin_unlock(mgr->lock);
}

drm_connector_t *drm_connector_get(drm_resource_manager_t *mgr, uint32_t id) {
    spin_lock(mgr->lock);

    for (uint32_t i = 0; i < DRM_MAX_CONNECTORS_PER_DEVICE; i++) {
        if (mgr->connectors[i] && mgr->connectors[i]->id == id) {
            mgr->connectors[i]->refcount++;
            spin_unlock(mgr->lock);
            return mgr->connectors[i];
        }
    }

    spin_unlock(mgr->lock);
    return NULL;
}

// CRTC management
drm_crtc_t *drm_crtc_alloc(drm_resource_manager_t *mgr, void *driver_data) {
    spin_lock(mgr->lock);

    uint32_t slot = drm_find_free_slot((void **)mgr->crtcs, DRM_MAX_CRTCS_PER_DEVICE);
    if (slot == (uint32_t)-1) {
        spin_unlock(mgr->lock);
        return NULL;
    }

    drm_crtc_t *crtc = malloc(sizeof(drm_crtc_t));
    if (!crtc) {
        spin_unlock(mgr->lock);
        return NULL;
    }

    memset(crtc, 0, sizeof(drm_crtc_t));
    crtc->id = mgr->next_crtc_id++;
    crtc->driver_data = driver_data;
    crtc->refcount = 1;

    mgr->crtcs[slot] = crtc;
    spin_unlock(mgr->lock);

    return crtc;
}

void drm_crtc_free(drm_resource_manager_t *mgr, uint32_t id) {
    spin_lock(mgr->lock);

    for (uint32_t i = 0; i < DRM_MAX_CRTCS_PER_DEVICE; i++) {
        if (mgr->crtcs[i] && mgr->crtcs[i]->id == id) {
            if (--mgr->crtcs[i]->refcount == 0) {
                free(mgr->crtcs[i]);
                mgr->crtcs[i] = NULL;
            }
            break;
        }
    }

    spin_unlock(mgr->lock);
}

drm_crtc_t *drm_crtc_get(drm_resource_manager_t *mgr, uint32_t id) {
    spin_lock(mgr->lock);

    for (uint32_t i = 0; i < DRM_MAX_CRTCS_PER_DEVICE; i++) {
        if (mgr->crtcs[i] && mgr->crtcs[i]->id == id) {
            mgr->crtcs[i]->refcount++;
            spin_unlock(mgr->lock);
            return mgr->crtcs[i];
        }
    }

    spin_unlock(mgr->lock);
    return NULL;
}

// Encoder management
drm_encoder_t *drm_encoder_alloc(drm_resource_manager_t *mgr, uint32_t type, void *driver_data) {
    spin_lock(mgr->lock);

    uint32_t slot = drm_find_free_slot((void **)mgr->encoders, DRM_MAX_ENCODERS_PER_DEVICE);
    if (slot == (uint32_t)-1) {
        spin_unlock(mgr->lock);
        return NULL;
    }

    drm_encoder_t *encoder = malloc(sizeof(drm_encoder_t));
    if (!encoder) {
        spin_unlock(mgr->lock);
        return NULL;
    }

    memset(encoder, 0, sizeof(drm_encoder_t));
    encoder->id = mgr->next_encoder_id++;
    encoder->type = type;
    encoder->driver_data = driver_data;
    encoder->refcount = 1;

    mgr->encoders[slot] = encoder;
    spin_unlock(mgr->lock);

    return encoder;
}

void drm_encoder_free(drm_resource_manager_t *mgr, uint32_t id) {
    spin_lock(mgr->lock);

    for (uint32_t i = 0; i < DRM_MAX_ENCODERS_PER_DEVICE; i++) {
        if (mgr->encoders[i] && mgr->encoders[i]->id == id) {
            if (--mgr->encoders[i]->refcount == 0) {
                free(mgr->encoders[i]);
                mgr->encoders[i] = NULL;
            }
            break;
        }
    }

    spin_unlock(mgr->lock);
}

drm_encoder_t *drm_encoder_get(drm_resource_manager_t *mgr, uint32_t id) {
    spin_lock(mgr->lock);

    for (uint32_t i = 0; i < DRM_MAX_ENCODERS_PER_DEVICE; i++) {
        if (mgr->encoders[i] && mgr->encoders[i]->id == id) {
            mgr->encoders[i]->refcount++;
            spin_unlock(mgr->lock);
            return mgr->encoders[i];
        }
    }

    spin_unlock(mgr->lock);
    return NULL;
}

// Framebuffer management
drm_framebuffer_t *drm_framebuffer_alloc(drm_resource_manager_t *mgr, void *driver_data) {
    spin_lock(mgr->lock);

    uint32_t slot = drm_find_free_slot((void **)mgr->framebuffers, DRM_MAX_FRAMEBUFFERS_PER_DEVICE);
    if (slot == (uint32_t)-1) {
        spin_unlock(mgr->lock);
        return NULL;
    }

    drm_framebuffer_t *fb = malloc(sizeof(drm_framebuffer_t));
    if (!fb) {
        spin_unlock(mgr->lock);
        return NULL;
    }

    memset(fb, 0, sizeof(drm_framebuffer_t));
    fb->id = mgr->next_framebuffer_id++;
    fb->driver_data = driver_data;
    fb->refcount = 1;

    mgr->framebuffers[slot] = fb;
    spin_unlock(mgr->lock);

    return fb;
}

void drm_framebuffer_free(drm_resource_manager_t *mgr, uint32_t id) {
    spin_lock(mgr->lock);

    for (uint32_t i = 0; i < DRM_MAX_FRAMEBUFFERS_PER_DEVICE; i++) {
        if (mgr->framebuffers[i] && mgr->framebuffers[i]->id == id) {
            if (--mgr->framebuffers[i]->refcount == 0) {
                free(mgr->framebuffers[i]);
                mgr->framebuffers[i] = NULL;
            }
            break;
        }
    }

    spin_unlock(mgr->lock);
}

drm_framebuffer_t *drm_framebuffer_get(drm_resource_manager_t *mgr, uint32_t id) {
    spin_lock(mgr->lock);

    for (uint32_t i = 0; i < DRM_MAX_FRAMEBUFFERS_PER_DEVICE; i++) {
        if (mgr->framebuffers[i] && mgr->framebuffers[i]->id == id) {
            mgr->framebuffers[i]->refcount++;
            spin_unlock(mgr->lock);
            return mgr->framebuffers[i];
        }
    }

    spin_unlock(mgr->lock);
    return NULL;
}

// Plane management
drm_plane_t *drm_plane_alloc(drm_resource_manager_t *mgr, void *driver_data) {
    spin_lock(mgr->lock);

    uint32_t slot = drm_find_free_slot((void **)mgr->planes, DRM_MAX_PLANES_PER_DEVICE);
    if (slot == (uint32_t)-1) {
        spin_unlock(mgr->lock);
        return NULL;
    }

    drm_plane_t *plane = malloc(sizeof(drm_plane_t));
    if (!plane) {
        spin_unlock(mgr->lock);
        return NULL;
    }

    memset(plane, 0, sizeof(drm_plane_t));
    plane->id = mgr->next_plane_id++;
    plane->driver_data = driver_data;
    plane->refcount = 1;

    mgr->planes[slot] = plane;
    spin_unlock(mgr->lock);

    return plane;
}

void drm_plane_free(drm_resource_manager_t *mgr, uint32_t id) {
    spin_lock(mgr->lock);

    for (uint32_t i = 0; i < DRM_MAX_PLANES_PER_DEVICE; i++) {
        if (mgr->planes[i] && mgr->planes[i]->id == id) {
            if (--mgr->planes[i]->refcount == 0) {
                free(mgr->planes[i]);
                mgr->planes[i] = NULL;
            }
            break;
        }
    }

    spin_unlock(mgr->lock);
}

drm_plane_t *drm_plane_get(drm_resource_manager_t *mgr, uint32_t id) {
    spin_lock(mgr->lock);

    for (uint32_t i = 0; i < DRM_MAX_PLANES_PER_DEVICE; i++) {
        if (mgr->planes[i] && mgr->planes[i]->id == id) {
            mgr->planes[i]->refcount++;
            spin_unlock(mgr->lock);
            return mgr->planes[i];
        }
    }

    spin_unlock(mgr->lock);
    return NULL;
}
