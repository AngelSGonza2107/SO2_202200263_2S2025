#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/string.h>

static DEFINE_MUTEX(context_list_lock);
static LIST_HEAD(context_list);

static atomic_t id_counter = ATOMIC_INIT(1);

// Estructura que representa un archivo a monitorear
// Esto con el fin de poder leer varios archivos en un solo contexto
struct log_file {
    struct file *file;
    char *path;
    loff_t last_pos;
    struct list_head list;
};

// Estructura del contexto de monitoreo. Contiene toda la información necesaria para el hilo de monitoreo
// - La lista de archivos a monitorear list_head files
// - El archivo central de logs log_file
// - La palabra clave a buscar keyword
// - Un mutex para proteger el acceso a la lista de archivos lock
// - Una wait queue para dormir el hilo waitq
// Finalmente un ID único id
struct thread_ctx {
    u32 id;
    struct list_head list;
    struct list_head files;
    struct mutex lock;
    struct task_struct *thread;
    wait_queue_head_t waitq;
    bool stop;
    struct file *log_file;
    char keyword[128];
    size_t keyword_len;
};

// Hilo del kernel que ejecuta el monitoreo para un contexto específico.
static int monitor_thread(void *data) {
    struct thread_ctx *ctx = data;
    struct log_file *fw;
    
    while (!kthread_should_stop() && !READ_ONCE(ctx->stop)) {
        mutex_lock(&ctx->lock);
        list_for_each_entry(fw, &ctx->files, list) {
            monitor_file(ctx, fw);
        }
        mutex_unlock(&ctx->lock);
        
        wait_event_interruptible_timeout(ctx->waitq, READ_ONCE(ctx->stop) || kthread_should_stop(), msecs_to_jiffies(2000));
    }
    return 0;
}

// Escribe un mensaje de log en el archivo central
static int write_to_central_log(struct thread_ctx *ctx, const char *src_path, const char *line, size_t len) {
    size_t path_len = strlen(src_path);
    size_t bufsize = path_len + ctx->keyword_len + len + 50;
    char *buffer = kmalloc(bufsize, GFP_KERNEL);
    if (!buffer) return -ENOMEM;

    size_t written = snprintf(buffer, bufsize, "%s - Palabra '%s' encontrada en el log '%.*s'\n",
                              src_path, ctx->keyword, (int)len, line);

    mutex_lock(&ctx->lock);
    loff_t pos = ctx->log_file->f_pos;
    int ret = kernel_write(ctx->log_file, buffer, written, &pos);
    if (ret > 0) {
        ctx->log_file->f_pos = pos;
        ret = 0;
    }
    mutex_unlock(&ctx->lock);

    kfree(buffer);
    return ret;
}

// Busca la palabra clave en un buffer de texto
static int contains_keyword(const char *line, size_t len, const char *keyword, size_t kwlen) {
    if (len < kwlen) return 0;
    for (size_t i = 0; i <= len - kwlen; i++) {
        if (!memcmp(line + i, keyword, kwlen))
            return 1;
    }
    return 0;
}

// Lee el archivo monitoreado y busca la palabra clave.
static int monitor_file(struct thread_ctx *ctx, struct log_file *fw) {
    loff_t size = i_size_read(file_inode(fw->file));
    char *buffer;
    ssize_t read_bytes;

    if (size <= fw->last_pos) return 0;

    buffer = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!buffer) return -ENOMEM;

    loff_t pos = fw->last_pos;
    size_t to_read = min_t(size_t, PAGE_SIZE, size - pos);
    
    read_bytes = kernel_read(fw->file, buffer, to_read, &pos);
    if (read_bytes > 0) {
        if (contains_keyword(buffer, read_bytes, ctx->keyword, ctx->keyword_len)) {
            write_to_central_log(ctx, fw->path, buffer, read_bytes);
        }
    }
    fw->last_pos = pos;
    
    kfree(buffer);
    return 0;
}

// Añade un archivo a monitorear al contexto.
static int add_watched_file(struct thread_ctx *ctx, const char *path) {
    struct log_file *fw;
    struct file *file;
    
    file = filp_open(path, O_RDONLY, 0);
    if (IS_ERR(file)) return PTR_ERR(file);
    
    fw = kzalloc(sizeof(*fw), GFP_KERNEL);
    if (!fw) {
        fput(file);
        return -ENOMEM;
    }
    
    fw->file = file;
    fw->path = kstrdup(path, GFP_KERNEL);
    if (!fw->path) {
        fput(file);
        kfree(fw);
        return -ENOMEM;
    }
    
    fw->last_pos = i_size_read(file_inode(file));
    list_add_tail(&fw->list, &ctx->files);
    return 0;
}

// Syscall para iniciar el monitoreo.
SYSCALL_DEFINE3(start_log_watch,
                const char __user *const __user *, paths,
                const char __user *, log_path,
                const char __user *, keyword) {
    
    struct thread_ctx *ctx;
    char *k_keyword, *k_log_path;
    char *k_paths[5] = {0};
    int i, n_paths = 0;
    int id, ret = 0;
    
    // 1. Copia los argumentos del usuario.
    k_keyword = strndup_user(keyword, 128);
    if (IS_ERR(k_keyword)) return PTR_ERR(k_keyword);
    
    k_log_path = strndup_user(log_path, PATH_MAX);
    if (IS_ERR(k_log_path)) {
        ret = PTR_ERR(k_log_path);
        goto out_keyword;
    }
    
    for (n_paths = 0; n_paths < 5; n_paths++) {
        const char __user *user_path;
        if (copy_from_user(&user_path, &paths[n_paths], sizeof(user_path))) {
            ret = -EFAULT;
            goto out_paths;
        }
        if (!user_path) break;
        
        k_paths[n_paths] = strndup_user(user_path, PATH_MAX);
        if (IS_ERR(k_paths[n_paths])) {
            ret = PTR_ERR(k_paths[n_paths]);
            k_paths[n_paths] = NULL;
            goto out_paths;
        }
    }
    
    if (n_paths == 0) {
        ret = -EINVAL;
        goto out_paths;
    }
    
    // 2. Crea y configura el contexto de monitoreo.
    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx) {
        ret = -ENOMEM;
        goto out_paths;
    }
    
    INIT_LIST_HEAD(&ctx->files);
    mutex_init(&ctx->lock);
    init_waitqueue_head(&ctx->waitq);
    strscpy(ctx->keyword, k_keyword, 128);
    ctx->keyword_len = strlen(ctx->keyword);
    
    ctx->log_file = filp_open(k_log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (IS_ERR(ctx->log_file)) {
        ret = PTR_ERR(ctx->log_file);
        ctx->log_file = NULL;
        goto out_ctx;
    }
    
    for (i = 0; i < n_paths; i++) {
        ret = add_watched_file(ctx, k_paths[i]);
        if (ret) goto out_ctx;
    }
    
    // 3. Asigna un ID único y lo guarda
    ctx->id = atomic_fetch_add(1, &id_counter);

    mutex_lock(&context_list_lock);
    list_add_tail(&ctx->list, &context_list);
    mutex_unlock(&context_list_lock);
    
    if (id < 0) {
        ret = id;
        goto out_ctx;
    }
    ctx->id = id;
    
    // 4. Lanza el hilo del kernel.
    ctx->thread = kthread_run(monitor_thread, ctx, "log_watch_%u", id);
    if (IS_ERR(ctx->thread)) {
        ret = PTR_ERR(ctx->thread);
        mutex_lock(&context_list_lock);
        list_del(&ctx->list);
        mutex_unlock(&context_list_lock);
        goto out_ctx;
    }
    
    // 5. Libera la memoria temporal y retorna el ID.
out_paths:
    for (i = 0; i < n_paths; i++) kfree(k_paths[i]);
    kfree(k_log_path);
out_keyword:
    kfree(k_keyword);
    
    return ctx->id;

out_ctx:
    struct log_file *fw, *tmp;
    
    if (ctx->log_file) fput(ctx->log_file);
    
    list_for_each_entry_safe(fw, tmp, &ctx->files, list) {
        list_del(&fw->list);
        if (fw->file) fput(fw->file);
        kfree(fw->path);
        kfree(fw);
    }
    
    kfree(ctx);
    goto out_paths;
}

// Syscall para detener el monitoreo por ID.
SYSCALL_DEFINE1(stop_log_watch, u32, id) {
    struct thread_ctx *ctx;
    
    // 1. Busca el contexto usando el ID.
    mutex_lock(&context_list_lock);
    list_for_each_entry(ctx, &context_list, list) {
        if (ctx->id == id)
            break;
    }
    if (&ctx->list == &context_list) {
        mutex_unlock(&context_list_lock);
        return -ESRCH;
    }
    if (!ctx) {
        mutex_unlock(&context_list_lock);
        return -ESRCH; // Retorna si el ID no existe.
    }
    
    // 2. Si ya está detenido, retorna un error.
    if (READ_ONCE(ctx->stop)) {
        mutex_unlock(&context_list_lock);
        return -EALREADY;
    }
    
    // 3. Marca la bandera de detención y remueve el ID.
    WRITE_ONCE(ctx->stop, true);
    list_del(&ctx->list);
    mutex_unlock(&context_list_lock);
    
    // 4. Despierta el hilo para que se detenga y libera los recursos.
    wake_up_interruptible(&ctx->waitq);
    if (ctx->thread) kthread_stop(ctx->thread);
    
    return 0;
}