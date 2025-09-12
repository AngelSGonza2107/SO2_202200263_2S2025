#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/errno.h>

#define MAX_MSG_SIZE 512
#define MAX_QUEUE_SIZE 10

// Estructura del mensaje
struct item {
    char dat[MAX_MSG_SIZE];     // Datos del mensaje
    u32 len;                    // Longitud del mensaje
    struct list_head node;      // Nodo para lista enlazada
};

// Cola de mensajes y sincronización
static LIST_HEAD(data_queue);                       // Lista doblemente enlazada para mensajes
static DEFINE_MUTEX(queue_mutex);                   // Mutex para acceso concurrente
static DECLARE_WAIT_QUEUE_HEAD(post_wait_queue);    // Procesos esperando enviar
static DECLARE_WAIT_QUEUE_HEAD(get_wait_queue);     // Procesos esperando recibir

static int current_queue_size = 0;             // Tamaño actual de la cola

// Syscall para enviar mensaje al canal
SYSCALL_DEFINE2(ipc_channel_send, const char __user *, dat, u32, len)
{
    struct item *it;
    u32 n;
    int ret = 0;

    if (!dat || len == 0)
        return -EINVAL;

    // Limitar tamaño del mensaje
    n = (len > MAX_MSG_SIZE) ? MAX_MSG_SIZE : len;

    // Esperar a que haya espacio en la cola
    while (1) {
        mutex_lock(&queue_mutex);
        if (current_queue_size < MAX_QUEUE_SIZE)
            break;
        mutex_unlock(&queue_mutex);

        if (wait_event_interruptible(post_wait_queue, current_queue_size < MAX_QUEUE_SIZE))
            return -ERESTARTSYS;
    }

    // Reservar memoria en kernel
    it = kmalloc(sizeof(*it), GFP_KERNEL);
    if (!it) {
        ret = -ENOMEM;
        goto exit;
    }

    it->len = n;

    // Copiar mensaje desde user space
    if (copy_from_user(it->dat, dat, n)) {
        kfree(it);
        ret = -EFAULT;
        goto exit;
    }

    // Agregar a la cola
    list_add_tail(&it->node, &data_queue);
    current_queue_size++;

exit:
    mutex_unlock(&queue_mutex);
    if (ret == 0)
        wake_up_interruptible(&get_wait_queue);
    return (ret == 0) ? n : ret;
}

// Syscall para recibir mensaje del canal
SYSCALL_DEFINE2(ipc_channel_receive, char __user *, dat, u32, len)
{
    struct item *it;
    u32 n;
    int ret = 0;

    if (!dat || len == 0)
        return -EINVAL;

    // Esperar a que haya mensajes
    while (1) {
        mutex_lock(&queue_mutex);
        if (!list_empty(&data_queue))
            break;
        mutex_unlock(&queue_mutex);

        if (wait_event_interruptible(get_wait_queue, !list_empty(&data_queue)))
            return -ERESTARTSYS;
    }

    // Obtener primer mensaje
    it = list_first_entry(&data_queue, struct item, node);
    list_del(&it->node);
    current_queue_size--;

    // Limitar cantidad de datos copiados al buffer del usuario
    n = (it->len > len) ? len : it->len;

    // Copiar mensaje al espacio de usuario
    if (copy_to_user(dat, it->dat, n)) {
        kfree(it);
        ret = -EFAULT;
        goto exit;
    }

    kfree(it);

exit:
    mutex_unlock(&queue_mutex);
    if (ret == 0)
        wake_up_interruptible(&post_wait_queue);
    return (ret == 0) ? n : ret;
}
