#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/fb.h>

// fb_info: estructura que contiene información sobre el framebuffer
extern struct fb_info *registered_fb[];
// num_registered_fb: número de framebuffers registrados
extern int num_registered_fb;

#include <drm/drm_device.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_modes.h>
#include <drm/drm_mode_config.h>
#include <drm/drm_fb_helper.h>

// Conceptos claves
// - DRM: (Direct Rendering Manager) es un subsistema de Linux que gestiona la representación gráfica en la pantalla.
// - helper DRM: proporciona funciones auxiliares para facilitar la integración de framebuffer con DRM.
// - drm_device: estructura que representa un dispositivo gráfico en el subsistema DRM.
// - CRTC: componente del sistema gráfico que toma la imagen y establece la salida
// - drm_from_fb0(): busca el dispositivo DRM asociado al framebuffer 0 (/dev/fb0)

// Devuelve el dispositivo DRM asociado al framebuffer 0
static struct drm_device *drm_from_fb0(void) {
    struct fb_info *framebuffer_info;
    struct drm_fb_helper *framebuffer_helper;

    if (num_registered_fb <= 0) {
        return NULL;
    }

    framebuffer_info = registered_fb[0];
    if (!framebuffer_info || !framebuffer_info->par) {
        return NULL;
    }

    framebuffer_helper = (struct drm_fb_helper *)framebuffer_info->par;
    return (framebuffer_helper && framebuffer_helper->dev) ? framebuffer_helper->dev : NULL;
}

// Busca la resolución activa del dispositivo DRM
static int drm_active_mode(struct drm_device *drm, int *w, int *h) {
    struct drm_connector *connector;
    struct drm_connector_list_iter list_connectors;

    if (!drm || !w || !h) {
        return -EINVAL;
    }

    *w = 0;
    *h = 0;

    drm_connector_list_iter_begin(drm, &list_connectors);

    drm_for_each_connector_iter(connector, &list_connectors) {
        const struct drm_connector_state *state_conector = connector->state;
        const struct drm_crtc *screen_controller;
        const struct drm_crtc_state *screen_controller_state;

        if (connector->status != connector_status_connected || !state_conector) {
            continue;
        }

        screen_controller = state_conector->crtc;
        if (!screen_controller) {
            continue;
        }

        screen_controller_state = screen_controller->state;
        if (!screen_controller_state || !screen_controller_state->enable) {
            continue;
        }

        if (screen_controller_state->mode.hdisplay > 0 && screen_controller_state->mode.vdisplay > 0) {
            *w = screen_controller_state->mode.hdisplay;
            *h = screen_controller_state->mode.vdisplay;
            break;
        }
    }

    drm_connector_list_iter_end(&list_connectors);

    return (*w > 0 && *h > 0) ? 0 : -ENODEV;
}

// Syscall para obtener la resolución de pantalla
SYSCALL_DEFINE2(get_screen_resolution, int __user *, width, int __user *, height) {
    int w = 0, h = 0;
    int width_value, height_value;
    int result;

    if (!width || !height)
        return -EINVAL;

    result = drm_active_mode(drm_from_fb0(), &w, &h);
    if (result) {
        return result;
    }

    width_value = copy_to_user(width, &w, sizeof(w));
    height_value = copy_to_user(height, &h, sizeof(h));
    if (width_value || height_value) {
        return -EFAULT;
    }

    return 0;
}