#include <linux/init.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/printk.h>

// - KEY_CNT es el numero de teclas soportadas por el dispositivo
// - input_report_key se utiliza para reportar eventos de pulsación de teclas
// - input_sync se utiliza para sincronizar los eventos de entrada

// Variable global para el dispositivo del teclado virtual
static struct input_dev *virtual_kbd_dev;

// Simula la pulsación de una tecla en el teclado virtual
static void virtual_kbd_simulate_key_press(int keycode){
    input_report_key(virtual_kbd_dev, keycode, 1);
    input_sync(virtual_kbd_dev);
    input_report_key(virtual_kbd_dev, keycode, 0);
    input_sync(virtual_kbd_dev);
}

// Funcion para registrar dispositivo de entrada virtual
static int __init virtual_kbd_init(void){
    int i, err;

    // Reserva de memoria para el dispositivo de entrada virtual
    virtual_kbd_dev = input_allocate_device();
    if(!virtual_kbd_dev){
        return -ENOMEM;
    }

    // Configuraciones del dispositivo de entrada virtual
    virtual_kbd_dev->name = "Virtual Keyboard";
    virtual_kbd_dev->phys = "virtual/input0";
    virtual_kbd_dev->id.bustype = BUS_VIRTUAL;
    virtual_kbd_dev->id.vendor = 0x1234;
    virtual_kbd_dev->id.product = 0x5678;
    virtual_kbd_dev->id.version = 1;

    // Establecer los eventos y tipos de eventos que soporta el dispositivo
    // evbit tipo de eventos que puede procesar el dispositivo
    __set_bit(EV_KEY, virtual_kbd_dev->evbit);
    // Establecer los keycodes que soporta el dispositivo
    for(i = 0; i < KEY_CNT; i++){
        __set_bit(i, virtual_kbd_dev->keybit); 
    }

    // Registrar el dispositivo de entrada virtual
    err = input_register_device(virtual_kbd_dev);
    // Si hay un error, se libera el dispositivo
    if(err){
        input_free_device(virtual_kbd_dev);
        virtual_kbd_dev = NULL;
        return err;
    }

    pr_info("Virtual keyboard initialized\n");
    return 0;
}

// Definicion de la syscall
SYSCALL_DEFINE1(send_key_event, int, keycode){
    // Validacion del keycode mediante un rango
    if(keycode < 0 || keycode >= KEY_CNT){
        return -EINVAL;
    }
    // Verificacion de la disponibilidad del dispositivo de teclado virtual
    if (!virtual_kbd_dev) {
        return -ENODEV;
    }

    // Simulacion de pulsación de la tecla
    virtual_kbd_simulate_key_press(keycode);
    return 0;
}

// Inicializar el mouse virtual en el subsistema de entrada
late_initcall(virtual_kbd_init);