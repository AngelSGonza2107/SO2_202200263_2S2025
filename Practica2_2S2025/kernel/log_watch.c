#include <linux/syscalls_usac.h> // Usamos tu header
#include <linux/delay.h>

// Estructura para mantener el estado de cada archivo monitoreado (su última posición de lectura)
struct file_state {
    char *path;
    loff_t pos;
};

// Modificamos la estructura principal para incluir el estado de los archivos
struct log_watch {
    struct task_struct *thread;
    struct list_head list;
    bool stop;
    int id;
    struct mutex lock;

    // Parámetros copiados al kernel
    int num_files;
    struct file_state *files; // Array con las rutas y sus posiciones
    char *central_log;
    char *keyword;
};

static LIST_HEAD(log_watch_list);
static DEFINE_MUTEX(global_list_lock);
static atomic_t next_id = ATOMIC_INIT(1); // Usamos atomic_t para seguridad en concurrencia

// Hilo de monitoreo mejorado
static int watch_thread(void *data) {
    struct log_watch *lw = (struct log_watch *)data;
    char *buf;
    int i;

    // Buffer de 2KB para leer los logs
    buf = kmalloc(2048, GFP_KERNEL);
    if (!buf) {
        pr_err("log_watch: No se pudo asignar memoria para el buffer de lectura\n");
        return -ENOMEM;
    }

    // El bucle principal se detiene si se llama a kthread_stop()
    while (!kthread_should_stop()) {
        for (i = 0; i < lw->num_files; i++) {
            struct file *f;
            ssize_t bytes_read;

            // Abrimos el archivo a monitorear
            f = filp_open(lw->files[i].path, O_RDONLY, 0);
            if (IS_ERR(f)) continue;

            // Leemos desde la última posición conocida
            while ((bytes_read = kernel_read(f, buf, 2047, &lw->files[i].pos)) > 0) {
                // CORRECCIÓN: Usamos bytes_read para terminar la cadena correctamente
                buf[bytes_read] = '\0';

                // Si encontramos la palabra clave
                if (strstr(buf, lw->keyword)) {
                    struct file *central;
                    char outbuf[2560]; // Buffer más grande para el mensaje de salida
                    loff_t central_pos = 0;

                    // Abrimos el log central en modo de añadir (append)
                    central = filp_open(lw->central_log, O_WRONLY | O_APPEND | O_CREAT, 0644);
                    if (IS_ERR(central)) {
                        pr_warn("log_watch: No se pudo abrir el log central %s\n", lw->central_log);
                        continue;
                    }
                    
                    // Creamos el mensaje a escribir
                    snprintf(outbuf, sizeof(outbuf),
                             "[ORIGEN: %s] [KEYWORD: %s] --- LOG: %s\n",
                             lw->files[i].path, lw->keyword, buf);
                    
                    // Escribimos en el log central
                    kernel_write(central, outbuf, strlen(outbuf), &central->f_pos);
                    filp_close(central, NULL);
                }
            }
            filp_close(f, NULL);
        }
        // Esperamos 2 segundos antes de la siguiente revisión
        msleep(2000);
    }

    kfree(buf);
    pr_info("log_watch: Hilo %d detenido.\n", lw->id);
    return 0;
}

// ------ Syscalls ------

SYSCALL_DEFINE1(start_log_watch, struct log_watch_params __user *, user_params) {
    struct log_watch *lw;
    struct log_watch_params params_from_user;
    int i;

    // 1. Asignar memoria en el kernel para la estructura principal
    lw = kzalloc(sizeof(struct log_watch), GFP_KERNEL);
    if (!lw) return -ENOMEM;

    // 2. Copiar la estructura de parámetros (que contiene punteros) desde el usuario
    if (copy_from_user(&params_from_user, user_params, sizeof(struct log_watch_params))) {
        kfree(lw);
        return -EFAULT;
    }
    
    lw->num_files = params_from_user.num_files;

    // 3. Copiar la RUTA DEL LOG CENTRAL (string) del usuario al kernel
    lw->central_log = kstrndup_from_user(params_from_user.central_log, 256, GFP_KERNEL);
    if (IS_ERR(lw->central_log)) {
        kfree(lw);
        return PTR_ERR(lw->central_log);
    }

    // 4. Copiar la PALABRA CLAVE (string) del usuario al kernel
    lw->keyword = kstrndup_from_user(params_from_user.keyword, 64, GFP_KERNEL);
    if (IS_ERR(lw->keyword)) {
        kfree(lw->central_log);
        kfree(lw);
        return PTR_ERR(lw->keyword);
    }

    // 5. Asignar memoria para el array de estado de archivos
    lw->files = kcalloc(lw->num_files, sizeof(struct file_state), GFP_KERNEL);
    if (!lw->files) {
        kfree(lw->central_log);
        kfree(lw->keyword);
        kfree(lw);
        return -ENOMEM;
    }

    // 6. Copiar CADA RUTA DE ARCHIVO a monitorear
    for (i = 0; i < lw->num_files; i++) {
        char __user *user_path;
        if (get_user(user_path, &params_from_user.files[i])) {
            // Error, limpiar todo
            goto cleanup_files;
        }
        lw->files[i].path = kstrndup_from_user(user_path, 256, GFP_KERNEL);
        if (IS_ERR(lw->files[i].path)) {
            // Error, limpiar todo
            goto cleanup_files;
        }
        lw->files[i].pos = 0; // Posición inicial es 0
    }

    // 7. Inicializar y añadir a la lista global
    mutex_init(&lw->lock);
    lw->id = atomic_inc_return(&next_id);
    lw->stop = false;

    mutex_lock(&global_list_lock);
    list_add_tail(&lw->list, &log_watch_list);
    mutex_unlock(&global_list_lock);

    // 8. Iniciar el hilo
    lw->thread = kthread_run(watch_thread, lw, "log_watch_%d", lw->id);
    if (IS_ERR(lw->thread)) {
        // ... manejo de error ...
        return PTR_ERR(lw->thread);
    }

    return lw->id;

cleanup_files:
    // Función de limpieza en caso de error durante el bucle
    for (i = i - 1; i >= 0; i--) {
        kfree(lw->files[i].path);
    }
    kfree(lw->files);
    kfree(lw->central_log);
    kfree(lw->keyword);
    kfree(lw);
    return -EFAULT;
}

SYSCALL_DEFINE1(stop_log_watch, int, id) {
    struct log_watch *lw, *tmp;
    int found = 0;
    int i;

    mutex_lock(&global_list_lock);
    // CORRECCIÓN: Usamos list_for_each_entry_safe para encontrar y eliminar en un solo paso
    list_for_each_entry_safe(lw, tmp, &log_watch_list, list) {
        if (lw->id == id) {
            mutex_lock(&lw->lock);
            if (!lw->stop) {
                lw->stop = true;
                kthread_stop(lw->thread); // Esto detiene el hilo y espera a que termine

                // Liberar toda la memoria asignada
                kfree(lw->central_log);
                kfree(lw->keyword);
                for (i = 0; i < lw->num_files; i++) {
                    kfree(lw->files[i].path);
                }
                kfree(lw->files);
                
                list_del(&lw->list); // Quitar de la lista
                kfree(lw); // Liberar la estructura principal
                
                found = 1;
            }
            mutex_unlock(&lw->lock); // Liberamos el mutex de la instancia
            break; // Salimos del bucle
        }
    }
    mutex_unlock(&global_list_lock);

    return found ? 0 : -EINVAL; // Retorna 0 si se encontró, o error si no
}