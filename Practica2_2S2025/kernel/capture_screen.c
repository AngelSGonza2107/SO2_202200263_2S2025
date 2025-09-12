#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/fb.h>
#include <drm/drm_device.h>
#include <drm/drm_crtc.h>
#include <drm/drm_plane.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_modeset_lock.h>
#include <linux/iosys-map.h>

extern struct fb_info *registered_fb[];
extern int num_registered_fb;

// Estructura para pasar datos entre el espacio de usuario y el kernel
struct capture_struct {
    __u64 data_pointer;      // Dirección de memoria del usuario para la captura
    __u64 data_size;         // Tamaño máximo del buffer del usuario
    __u32 width;             // Ancho de la imagen
    __u32 height;            // Alto de la imagen
    __u32 bytes_per_row;     // Bytes por fila
    __u32 bytes_per_pixel;   // Bytes por pixel
    __u32 total_bytes;       // Tamaño real de la captura
};

/**
 * Obtiene el dispositivo DRM asociado al framebuffer /dev/fb0
 *
 * regresa un puntero al struct drm_device o NULL si no se encuentra
 */
static struct drm_device *get_drm_device_from_fb0(void) {
    // Verifica si hay framebuffers registrados y si el primero existe
    if (num_registered_fb <= 0 || !registered_fb[0])
        return NULL;

    struct fb_info *info = registered_fb[0];
    // Intenta obtener el helper de fb y luego el dispositivo DRM
    struct drm_fb_helper *helper = (struct drm_fb_helper *)info->par;
    return helper ? helper->dev : NULL;
}

/**
 * Busca el framebuffer primario y su CRTC
 *
 * drm: Dispositivo DRM
 * crtc_out: Puntero para almacenar el CRTC encontrado
 * fb_out: Puntero para almacenar el framebuffer encontrado
 * w: Puntero para almacenar el ancho
 * h: Puntero para almacenar el alto
 * devuelve 0 si tiene éxito
 */
static int get_primary_fb(struct drm_device *drm, struct drm_crtc **crtc_out,
                          struct drm_framebuffer **fb_out, u32 *w, u32 *h) {
    struct drm_modeset_acquire_ctx ctx;
    int ret;

    // Inicializa el contexto para bloquear el DRM
    drm_modeset_acquire_init(&ctx, 0);

    // Bucle para intentar obtener los bloqueos DRM
retry:
    ret = drm_modeset_lock_all_ctx(drm, &ctx);
    if (ret == -EDEADLK) {
        drm_modeset_backoff(&ctx);
        goto retry;
    }
    if (ret)
        goto out;

    // Itera sobre los conectores para encontrar el CRTC activo
    struct drm_connector *conn;
    struct drm_crtc *crtc = NULL;
    struct drm_connector_list_iter iter;
    drm_connector_list_iter_begin(drm, &iter);
    drm_for_each_connector_iter(conn, &iter) {
        if (conn->status == connector_status_connected && conn->state && conn->state->crtc) {
            crtc = conn->state->crtc;
            break;
        }
    }
    drm_connector_list_iter_end(&iter);

    if (!crtc) {
        ret = -ENODEV;
        goto unlock;
    }

    // Busca el plane (imagen lista para enviar a CRTC) primario asociado al CRTC
    struct drm_plane *plane;
    drm_for_each_plane(plane, drm) {
        if (plane->type == DRM_PLANE_TYPE_PRIMARY && plane->state &&
            plane->state->fb && plane->state->crtc == crtc) {
            // Se encontró el framebuffer primario
            drm_framebuffer_get(plane->state->fb);
            *crtc_out = crtc;
            *fb_out = plane->state->fb;
            *w = crtc->state->mode.hdisplay;
            *h = crtc->state->mode.vdisplay;
            ret = 0;
            goto unlock;
        }
    }
    ret = -ENODEV;

unlock:
    drm_modeset_drop_locks(&ctx);
out:
    drm_modeset_acquire_fini(&ctx);
    return ret;
}

/**
 * Implementa la llamada al sistema para capturar la pantalla
 *
 * user_c_struct: Puntero a la estructura de usuario con los parámetros
 * devuelve 0 si tiene éxito, o un código de error
 */
SYSCALL_DEFINE1(capture_screen, struct capture_struct __user *, user_c_struct) {
    struct capture_struct c_struct;
    // Copia la estructura del espacio de usuario al kernel
    if (copy_from_user(&c_struct, user_c_struct, sizeof(c_struct)))
        return -EFAULT;

    // Obtiene el dispositivo DRM.
    struct drm_device *drm = get_drm_device_from_fb0();
    if (!drm)
        return -ENODEV;

    // Busca el framebuffer principal
    struct drm_crtc *crtc = NULL;
    struct drm_framebuffer *fb = NULL;
    u32 width, height;
    int ret = get_primary_fb(drm, &crtc, &fb, &width, &height);
    if (ret)
        return ret;

    u32 pitch = fb->pitches[0];
    u32 bpp = fb->format->cpp[0] * 8;
    size_t total_bytes = (size_t)pitch * height;

    // Si el buffer de usuario es demasiado pequeño, se devuelve el tamaño requerido
    if (c_struct.data_size < total_bytes) {
        c_struct.width = width;
        c_struct.height = height;
        c_struct.bytes_per_row = pitch;
        c_struct.bytes_per_pixel = bpp / 8;
        c_struct.total_bytes = total_bytes;
        copy_to_user(user_c_struct, &c_struct, sizeof(c_struct));
        drm_framebuffer_put(fb);
        return -ENOSPC;
    }

    // Mapea la memoria del framebuffer al espacio de direcciones del kernel
    struct iosys_map map[DRM_FORMAT_MAX_PLANES];
    ret = drm_gem_fb_vmap(fb, map, NULL);
    if (ret) {
        drm_framebuffer_put(fb);
        return ret;
    }

    const u8 *vbase = (const u8 *)map[0].vaddr + fb->offsets[0];
    
    // Asigna un buffer temporal en el kernel y copia los datos
    void *tmp = vmalloc(total_bytes);
    if (!tmp) {
        ret = -ENOMEM;
        goto vunmap;
    }

    // Copia los datos del framebuffer al buffer temporal
    for (u32 y = 0; y < height; ++y) {
        memcpy((u8 *)tmp + (size_t)pitch * y, vbase + (size_t)pitch * y, pitch);
    }

    // Copia los datos capturados del buffer temporal al espacio de usuario
    if (copy_to_user((void __user *)(uintptr_t)c_struct.data_pointer, tmp, total_bytes)) {
        ret = -EFAULT;
        goto vfree;
    }

    // Actualiza la estructura de usuario con los valores reales
    c_struct.width = width;
    c_struct.height = height;
    c_struct.bytes_per_row = pitch;
    c_struct.bytes_per_pixel = bpp / 8;
    c_struct.total_bytes = total_bytes;
    
    // Copia la estructura de vuelta al espacio de usuario
    if (copy_to_user(user_c_struct, &c_struct, sizeof(c_struct)))
        ret = -EFAULT;

// Si alguna función falla, se salta a las etiquetas para liberar recursos
// Libera meemoria si se uso vmalloc()
vfree:
    vfree(tmp);
// Desmapea la memoria del framebuffer si se uso drm_gem_fb_vmap()
vunmap:
    drm_gem_fb_vunmap(fb, map);
    drm_framebuffer_put(fb);
    return ret;
}