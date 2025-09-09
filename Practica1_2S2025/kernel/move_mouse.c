#include <linux/init.h>         
#include <linux/input.h>       
#include <linux/printk.h>      
#include <linux/syscalls.h>    
#include <linux/mutex.h>       
#include <linux/capability.h>  
#include <linux/errno.h> 

// Variable global para el dispositivo del mouse virtual
static struct input_dev *virtual_mouse;

// Estructura para la exclusión mutua
// Mutex para proteger el acceso al dispositivo de entrada virtual
static DEFINE_MUTEX(vmouse_lock);

// Función de inicialización del módulo
static int __init mouse_syscall_init(void){
    int err;

    pr_info("vmouse_syscall: init start\n");

    // input_allocate_device() crea una nueva estructura input_dev
    // que representa un dispositivo de entrada virtual
    virtual_mouse = input_allocate_device();

    if (!virtual_mouse) {
        pr_err("vmouse_syscall: no se pudo alocar input_dev\n");
        return -ENOMEM;
    }

    // Configuración del dispositivo de entrada virtual
    virtual_mouse->name = "virtual_mouse";
    virtual_mouse->phys = "vmd/input0";
    virtual_mouse->id.bustype = BUS_VIRTUAL;
    virtual_mouse->id.vendor  = 0x0007;  
    virtual_mouse->id.product = 0x0009;
    virtual_mouse->id.version = 0x0001;

    // Establecer la propiedad de "puntero" en el dispositivo virtual_mouse
    // Esto indica al kernel que este dispositivo se comporta como un mouse
    __set_bit(INPUT_PROP_POINTER, virtual_mouse->propbit);

    // Macro: input_set_capability
    // Habilita el evento de movimiento relativo en el eje X para el dispositivo virtual_mouse
    input_set_capability(virtual_mouse, EV_REL, REL_X);

    // Macro: input_set_capability
    // Habilita el evento de movimiento relativo en el eje Y para el dispositivo virtual_mouse
    input_set_capability(virtual_mouse, EV_REL, REL_Y);

    // Macro: input_set_capability
    // Habilita el evento de clic izquierdo para el dispositivo virtual_mouse
    input_set_capability(virtual_mouse, EV_KEY, BTN_LEFT);

    // Macro: input_register_device
    // Registra el dispositivo de entrada virtual en el subsistema de entrada
    err = input_register_device(virtual_mouse);

    if (err) {
        pr_err("vmouse_syscall: no se pudo registrar input_dev (%d)\n", err);
        input_free_device(virtual_mouse);
        virtual_mouse = NULL;
        return err;
    }

    pr_info("vmouse_syscall: dispositivo virtual listo\n");

    return 0;
}

// Definicion de la syscall para mover el mouse
SYSCALL_DEFINE2(move_mouse, int, dx, int, dy){
    // Verifica si el dispositivo virtual_mouse está disponible
    if (!virtual_mouse){
        return -ENODEV;
    }

    // Verifica si los valores de desplazamiento están dentro de un rango válido
    if (dx < -1000 || dx > 1000 || dy < -1000 || dy > 1000){
        return -EINVAL;
    }

    // Bloquea el mutex antes de modificar el dispositivo virtual_mouse
    mutex_lock(&vmouse_lock);

    // Macro: input_report_rel
    // Envia el movimiento relativo en el eje X para el dispositivo virtual_mouse
    input_report_rel(virtual_mouse, REL_X, dx);
    // Envia el movimiento relativo en el eje Y para el dispositivo virtual_mouse
    input_report_rel(virtual_mouse, REL_Y, dy);
    // Macro: input_sync
    // Sincroniza los eventos de entrada para el dispositivo virtual_mouse
    input_sync(virtual_mouse);
    // Desbloquea el mutex
    mutex_unlock(&vmouse_lock);

    pr_info("vmouse_syscall: Mouse movido en las coordenadas: X=%d, Y=%d\n", dx, dy);

    return 0;
}

// Inicializar el mouse virtual en el subsistema de entrada
late_initcall(mouse_syscall_init);
