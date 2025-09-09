#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/printk.h>
#include <linux/mutex.h> // Necesario para el mutex

// Cabeceras DRM necesarias
#include <drm/drm_drv.h>
#include <drm/drm_device.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_fourcc.h>

// Símbolos externos para el método antiguo
extern struct mutex drm_core_lock;
extern struct list_head drm_device_list;

// Incluye la definición de la estructura (versión del kernel)
struct screen_capture {
    __u32 width;
    __u32 height;
    __u32 bpp;
    __u64 buffer_size;
    void __user *buffer;
};

SYSCALL_DEFINE1(capture_screen, struct screen_capture __user *, user_capture)
{
    struct screen_capture k_capture;
    struct drm_device *drm_dev = NULL;
    struct drm_device *found_drm_dev = NULL; // Para gestionar la referencia
    struct drm_framebuffer *fb = NULL;
    struct drm_gem_object *gem_obj = NULL;
    void *vaddr = NULL;
    size_t fb_size;
    int ret = 0;

    if (copy_from_user(&k_capture, user_capture, sizeof(k_capture))) {
        return -EFAULT;
    }

    if (!k_capture.buffer || k_capture.buffer_size == 0) {
        return -EINVAL;
    }

    // --- INICIO DE LA LÓGICA PARA KERNELS ANTIGUOS (4.x) ---
    // 1. Bloquear la lista de dispositivos para una iteración segura
    mutex_lock(&drm_core_lock);

    // 2. Iterar sobre la lista global de dispositivos DRM
    list_for_each_entry(drm_dev, &drm_device_list, dev_list) {
        struct drm_connector *connector;
        struct drm_connector_list_iter conn_iter;

        // 3. Incrementar el contador de referencias manualmente
        drm_dev_get(drm_dev);

        drm_connector_list_iter_begin(drm_dev, &conn_iter);
        drm_for_each_connector_iter(connector, &conn_iter) {
            if (connector->state && connector->state->crtc) {
                struct drm_crtc *crtc = connector->state->crtc;
                if (crtc->primary && crtc->primary->state && crtc->primary->state->fb) {
                    fb = crtc->primary->state->fb;
                    found_drm_dev = drm_dev; // Guardamos el dispositivo que encontramos
                    goto found_and_unlock;
                }
            }
        }
        drm_connector_list_iter_end(&conn_iter);

        // Si no se encontró, liberamos la referencia de este dispositivo
        drm_dev_put(drm_dev);
    }
    // Si terminamos el bucle sin encontrar nada, desbloqueamos y salimos
    mutex_unlock(&drm_core_lock);
    pr_err("capture_screen: No se encontró un framebuffer activo.\n");
    return -ENODEV;

found_and_unlock:
    // 4. Desbloquear la lista tan pronto como sea posible
    mutex_unlock(&drm_core_lock);
    // --- FIN DE LA LÓGICA PARA KERNELS ANTIGUOS ---

    // 5. Calcular tamaño y validar buffer
    fb_size = (size_t)fb->width * fb->height * (fb->bits_per_pixel / 8);

    if (k_capture.buffer_size < fb_size) {
        ret = -EINVAL;
        goto cleanup;
    }
    
    // 6. Obtener objeto GEM y mapearlo
    gem_obj = fb->obj[0];
    if (!gem_obj) {
        ret = -ENODEV;
        goto cleanup;
    }
    
    vaddr = drm_gem_object_vmap(gem_obj);
    if (IS_ERR(vaddr)) {
        ret = PTR_ERR(vaddr);
        vaddr = NULL; // Importante para el cleanup
        goto cleanup;
    }

    // 7. Copiar los datos al usuario
    if (copy_to_user(k_capture.buffer, vaddr, fb_size)) {
        ret = -EFAULT;
        goto cleanup;
    }

    // 8. Actualizar la estructura del usuario con la información
    k_capture.width = fb->width;
    k_capture.height = fb->height;
    // CORRECCIÓN: Usar el miembro directo de la estructura antigua
    k_capture.bpp = fb->bits_per_pixel;

    if (copy_to_user(user_capture, &k_capture, sizeof(k_capture))) {
        ret = -EFAULT;
        goto cleanup;
    }
    
    ret = 0; // Éxito

cleanup:
    if (vaddr) {
        drm_gem_object_vunmap(gem_obj, vaddr);
    }
    // 9. Liberar la referencia del dispositivo que encontramos
    if (found_drm_dev) {
        drm_dev_put(found_drm_dev);
    }
    
    return ret;
}