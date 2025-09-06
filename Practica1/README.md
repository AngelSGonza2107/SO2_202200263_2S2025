# 📄 Introducción al Kernel de Linux: Llamadas al Sistema

## 🧾 Descripción General

Este documento describe el proceso completo para la creación de syscalls personalizadas, las cuales permiten realizar diferentes funcinalidades básicas en el sistema, como el control del mouse, la obtención de información relevante de la pantalla y otros. Algunas de estas syscalls interactúan con dispositivos de entrada virtual registrado en el subsistema de entrada del kernel de Linux.

---

## ⚙️ Objetivos

Los objetivos de esta práctica son:

- Comprender la estructura y el funcionamiento básico del kernel Linux a través de la implementación práctica de llamadas al sistema.
- Crear nuevas llamadas al sistema con funcionalidades básicas para obtener información y estado del sistema operativo.
- Entender la importancia y utilidad de las llamadas al sistema en la interacción entre usuario y kernel.

---

## 🧩 Archivos Relevantes

- **Archivo del kernel (**`sys_move_mouse.c`**):** Contiene la lógica de inicialización del dispositivo virtual y la implementación de la syscall que permite simular el movimiento del puntero del mouse.
- **Archivo del kernel (**`sys_send_key_event.c`**):** Contiene la lógica de inicialización del dispositivo virtual y la implementación de la syscall que simula la pulsación de una tecla.
- **Archivo del kernel (**`sys_get_screen_resolution.c`**):** Contiene la implementación de la syscall que permite obtener la resolución actual de la pantalla principal.

---

## 📌 Detalles Técnicos

Para esta práctica se utilizó como base el código fuente de la versión longterm más reciente del kernel (**linux-6.12.41**), descargada desde kernel.org, y se compiló sobre una distribución Debian-based (**Linux Mint 21.1 XFCE**). Se cambió la versión del kernel para incluir mi nombre y número de carné como identificación. Para poder descargar dichos recursos se puede acceder a esta [carpeta Drive compartida](https://uedi.ingenieria.usac.edu.gt/campus/mod/url/view.php?id=195775).

### 🧠 Funcionalidad de la Syscall `sys_move_mouse`

```c
SYSCALL_DEFINE2(sys_move_mouse, int, dx, int, dy)
```

- **Parámetros:**
  - **dx**: Desplazamiento horizontal del cursor.
  - **dy**: Desplazamiento vertical del cursor.
- Utiliza `input_report_rel` para enviar eventos de movimiento relativo al subsistema de entrada.
- El movimiento es sincronizado con `input_sync`.

### 🛠️ Pasos Realizados

#### 1. **Inicialización del Dispositivo Virtual**

Se utilizó la función `core_initcall` para inicializar un dispositivo de entrada virtual (`input_dev`) durante el arranque del kernel:

```c
virtual_mouse = input_allocate_device();
```

Configuración del dispositivo:

- **Nombre**: `syscall_vmouse`
- **Tipo de eventos**:

  - Movimiento relativo: `REL_X`, `REL_Y`
  - Evento de botón: `BTN_LEFT`

Registro del dispositivo en el sistema de entrada:

```c
input_register_device(virtual_mouse);
```

#### 2. **Implementación de la Syscall**

Declaración de la syscall utilizando el macro `SYSCALL_DEFINE2` para evitar uso de headers y añadir archivos a la carpeta `include/`. El código:

- Valida que el dispositivo esté inicializado.
- Verifica que los valores de `dx` y `dy` no sean excesivos.
- Genera eventos de movimiento mediante:

```c
input_report_rel(virtual_mouse, REL_X, dx);
input_report_rel(virtual_mouse, REL_Y, dy);
input_sync(virtual_mouse);
```

#### 3. **Asignación del Número de Syscall**

Se definió el número de syscall como `463` en el espacio de usuario:

```c
#define SYS_MOVE_MOUSE 463
```

> ⚠️ Este número debe coincidir con el que se haya asignado en el archivo `syscall_64.tbl` del kernel.

#### 4. **Modificación de Archivos del Kernel**

Se modificaron los siguientes archivos para registrar la nueva syscall:

##### 📁 `arch/x86/entry/syscalls/syscall_64.tbl`

Agregar la entrada:

```
463     common   move_mouse     sys_move_mouse
```

##### 📁 `kernel/sys_move_mouse.c`

Agregar la implementación de la función `SYSCALL_DEFINE2`.

#### 5. **Compilación e Instalación del Kernel**

- Se recompiló el kernel utilizando `make` y `make modules_install install`.
- Se actualizó el gestor de arranque (`GRUB`) y se reinició en el nuevo kernel.

### 🧪 Prueba desde Espacio de Usuario

Se creó un programa en C para probar la syscall:

#### Código de prueba: `test_move_mouse.c`

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

#define SYS_MOVE_MOUSE 463

int main() {
    int dx = 20;
    int dy = 10;
    long res = syscall(SYS_MOVE_MOUSE, dx, dy);
    if (res == 0)
        printf("Movimiento del mouse simulado: dx=%d, dy=%d\n", dx, dy);
    else
        perror("Error en syscall move_mouse");
    return 0;
}
```

#### Compilación:

```bash
gcc test_move_mouse.c -o test_mouse
```

#### Ejecución:

```bash
sudo ./test_mouse
```

> 🔒 Se requiere privilegios de superusuario para acceder al sistema de entrada.

![move_mouse1](/Practica1/assets/mouse1.png)
![move_mouse2](/Practica1/assets/mouse2.png)

![evtest_mouse](/Practica1/assets/evtest1.png)

---

## 🧠 Funcionalidad de la Syscall `sys_send_key_event`

```c
SYSCALL_DEFINE1(send_key_event, int, keycode)
```

- **keycode**: Código de tecla a simular, basado en la enumeración `linux/input-event-codes.h` (por ejemplo, `KEY_A`, `KEY_ENTER`, etc.).
- La syscall genera un evento de pulsación de tecla (`keydown` y `keyup`) sobre un dispositivo de teclado virtual.
- Se utiliza `input_report_key` junto con `input_sync` para enviar los eventos al subsistema de entrada del kernel.

## 🛠️ Pasos Realizados

### 1. **Inicialización del Teclado Virtual**

Se inicializa un dispositivo de entrada virtual (`input_dev`) durante la fase de `late_initcall` del arranque del kernel:

```c
virtual_keyboard = input_allocate_device();
```

Configuración del dispositivo:

- **Nombre**: `syscall_vkeyboard`
- **Ubicación lógica**: `vkb/input0`
- **Bus simulado**: `BUS_USB`
- **Eventos soportados**:

  - Eventos de tecla: `EV_KEY`
  - Sincronización de eventos: `EV_SYN`

- **Teclas habilitadas**: Todas las del rango `0` a `KEY_MAX`

Registro del dispositivo:

```c
input_register_device(virtual_keyboard);
```

### 2. **Implementación de la Syscall**

La syscall se define con el macro:

```c
SYSCALL_DEFINE1(send_key_event, int, keycode)
```

Comportamiento:

- Verifica que el dispositivo de teclado virtual haya sido correctamente inicializado.
- Valida que el `keycode` esté dentro del rango válido (`0` a `KEY_MAX`).
- Simula una pulsación (`keydown`) y liberación (`keyup`) de la tecla especificada utilizando:

```c
input_report_key(virtual_keyboard, keycode, 1);
input_sync(virtual_keyboard);

input_report_key(virtual_keyboard, keycode, 0);
input_sync(virtual_keyboard);
```

- Registra el evento mediante `pr_info`.

### 3. **Asignación del Número de Syscall**

Se definió el número de syscall como `464` en el espacio de usuario:

```c
#define SYS_SEND_KEY_EVENT 464
```

> ⚠️ Este número debe coincidir con el que se haya asignado en el archivo `arch/x86/entry/syscalls/syscall_64.tbl` del kernel.

## 🧪 Prueba desde Espacio de Usuario

Se creó un programa en C para probar la syscall:

### Código de prueba: `test_send_key_event.c`

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

#define SYS_SEND_KEY_EVENT 464

#define KEY_A 30
#define KEY_B 48
#define KEY_ENTER 28

int main() {
    int keycode = KEY_A;

    long result = syscall(SYS_SEND_KEY_EVENT, keycode);

    if (result == 0) {
        printf("Syscall send_key_event ejecutada correctamente (keycode = %d)\n", keycode);
    } else {
        perror("Error en syscall send_key_event");
    }

    return 0;
}
```

![send_key_event](/Practica1/assets/key_event.png)

![evtest_key_event](/Practica1/assets/evtest2.png)

---

## 🧠 Funcionalidad de la Syscall `sys_get_screen_resolution`

```c
SYSCALL_DEFINE3(get_screen_resolution, int fd, int __user *width, int __user *height)
```

- **fd**: Descriptor de archivo asociado a un dispositivo DRM (como `/dev/dri/card0`).
- **width**: Puntero en espacio de usuario donde se copiará la resolución horizontal de la pantalla.
- **height**: Puntero en espacio de usuario donde se copiará la resolución vertical de la pantalla.
- La syscall obtiene la resolución actual de pantalla accediendo a la información del conector DRM asociado al dispositivo abierto. La resolución se obtiene de la estructura `drm_display_mode` y se devuelve al espacio de usuario mediante `copy_to_user()`.

---

## 🛠️ Pasos Realizados

### 1. **Acceso al Dispositivo DRM**

Se obtiene el descriptor de archivo del dispositivo DRM proporcionado por el usuario:

```c
file = fget(fd);
```

Luego, se valida que el archivo tenga datos privados válidos (`drm_file`), requeridos para acceder al dispositivo gráfico:

```c
if (!file->private_data) {
    fput(file);
    return -EINVAL;
}
```

> Se accede a la estructura `drm_device` a través de `drm_file->minor->dev`, permitiendo operar sobre el stack gráfico de DRM sin necesidad de ioctl.

---

### 2. **Bloqueo y Recorrido de Conectores DRM**

Se bloquean los modos de pantalla para evitar condiciones de carrera durante la lectura:

```c
drm_modeset_lock_all(dev);
```

Luego, se inicializa un iterador para recorrer los conectores de salida disponibles en el dispositivo:

```c
drm_connector_list_iter_begin(dev, &iter);
```

Durante la iteración, se busca el primer conector con estado "conectado":

```c
while ((conn = drm_connector_list_iter_next(&iter))) {
    if (conn->status != connector_status_connected)
        continue;
```

Y si existe un modo válido en el CRTC correspondiente, se obtiene la resolución:

```c
const struct drm_display_mode *m = &conn->state->crtc->state->mode;
if (m->hdisplay && m->vdisplay) {
    w = m->hdisplay;
    h = m->vdisplay;
    found = 1;
    break;
}
```

Finaliza la iteración y desbloquea los modos:

```c
drm_connector_list_iter_end(&iter);
drm_modeset_unlock_all(dev);
```

---

### 3. **Copia Segura de la Resolución al Espacio de Usuario**

Si se encontró un conector válido con resolución activa, los valores se copian al espacio de usuario:

```c
if (copy_to_user(width, &w, sizeof(int)) || copy_to_user(height, &h, sizeof(int)))
    return -EFAULT;
```

Si no se encontró un conector válido, se retorna `-ENODEV`.

---

### 4. **Asignación del Número de Syscall**

Se definió el número de syscall como `465` en el espacio de usuario:

```c
#define SYS_GET_SCREEN_RESOLUTION 465
```

> ⚠️ Este número debe coincidir con el definido en `arch/x86/entry/syscalls/syscall_64.tbl`.

---

## 🧪 Prueba desde Espacio de Usuario

Se creó un programa en C para invocar la syscall y mostrar la resolución:

### Código de prueba: `test_get_screen_resolution.c`

```c
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <errno.h>

#define SYS_GET_SCREEN_RESOLUTION 465

int main() {
    int fd = open("/dev/dri/card0", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    int width = 0, height = 0;

    long ret = syscall(SYS_GET_SCREEN_RESOLUTION, fd, &width, &height);
    if (ret == 0) {
        printf("Screen resolution: %dx%d\n", width, height);
    } else {
        perror("syscall get_screen_resolution");
        printf("Return value: %ld, errno: %d\n", ret, errno);
    }

    close(fd);
    return 0;
}
```

![get_screen_resolution](/Practica1/assets/resolucion.png)

---

## 🧼 Validaciones Realizadas

- Se validó que el dispositivo esté correctamente asignado antes de llamar a la syscall.
- Se estableció un rango máximo de valores permitidos (`dx`, `dy`) para evitar comportamientos erráticos.
- Se usaron `printk` en su formato simplificado para rastrear la ejecución en `dmesg`.

---

## ⛔ Errores encontrados

### ⚠️ Problema 1: **La syscall no aparecía al recompilar el kernel**

**Descripción:**
Después de implementar los nuevos archivos con los syscalls y modificar `syscall_table.S`, al recompilar el kernel, la nuevo syscall no era reconocida y causaba errores de compilación como:
`undefined reference to 'sys_move_mouse'`.

**Posible causa:**
La syscall fue implementada en un archivo nuevo, pero este archivo no fue agregado al `Makefile` correspondiente. A pesar de sí incluir correctamente a la tabla de llamadas al sistema (`syscall_64.tbl`).

**Solución:**

- Se aseguró que el archivo fuente con las nuevas funciones fuera agregado al Makefile en el directorio correspondiente (por ejemplo, `kernel/Makefile` o `arch/x86/entry/syscalls/Makefile`).

```makefile
...
# Makefile for the linux kernel.
#

obj-y     = fork.o exec_domain.o panic.o \
	    cpu.o exit.o softirq.o resource.o \
	    sysctl.o capability.o ptrace.o user.o \
	    signal.o sys.o umh.o workqueue.o pid.o task_work.o \
	    extable.o params.o \
	    kthread.o sys_ni.o nsproxy.o \
	    notifier.o ksysfs.o cred.o reboot.o \
	    async.o range.o smpboot.o ucount.o regset.o ksyms_common.o \
		move_mouse.o \
		send_key_event.o \
		get_screen_resolution.o

obj-$(CONFIG_USERMODE_DRIVER) += usermode_driver.o
obj-$(CONFIG_MULTIUSER) += groups.o
...
```

- Se verificó que la macro correspondiente estuviera en `arch/x86/entry/syscalls/syscall_64.tbl`.
- Se recompiló el kernel desde cero para asegurar que no quedaran archivos objetos viejos (`make clean && make`).

---

### ⚠️ Problema 2: **Movimiento del mouse no tenía efecto visible**

**Descripción:**
La llamada `sys_move_mouse(dx, dy)` se ejecutaba correctamente desde espacio de usuario sin lanzar errores, pero el puntero del mouse no mostraba ningún movimiento visible en pantalla.

**Causa inicial (descartada):**
Inicialmente se pensó que el problema estaba relacionado con una configuración de VMWare —específicamente, la opción **"Hide cursor on ungrab"**—, incluso se modificó el archivo de configuración de la máquina virtual para agregar **vmmouse.present = "FALSE"**. Pero esto **no era la causa real**.

**Causa real:**
El problema estaba en **el momento en que se registraba el dispositivo** dentro del kernel. El código usaba:

```c
core_initcall(mouse_syscall_init);
```

Esto hacía que el subsistema del mouse virtual se registrara demasiado temprano durante el arranque del sistema. Como resultado, **Xorg no lo reconocía como un dispositivo confiable**, ya que los dispositivos registrados muy pronto pueden ser ignorados por el servidor gráfico por no cumplir con ciertos criterios de inicialización o disponibilidad.

**Solución:**

Se cambió la macro de inicialización a:

```c
late_initcall(mouse_syscall_init);
```

Esto asegura que el dispositivo se registre en una etapa posterior del proceso de arranque, cuando el sistema ya tiene un contexto más completo y los subsistemas relevantes están activos. Tras este cambio, **Xorg comenzó a reconocer correctamente el dispositivo**, y los movimientos del cursor generados por `sys_move_mouse(dx, dy)` fueron visibles.

---

### ⚠️ Problema 3: **No se obtenía correctamente la resolución de pantalla**

**Descripción:**
La llamada `sys_get_screen_resolution(int *width, int *height)` devolvía siempre `0x0` como resolución o generaba valores incorrectos tras cambiar la configuración de pantalla.

**Causa real:**
Inicialmente se intentó obtener la resolución accediendo directamente al framebuffer a través del arreglo global `registered_fb[0]`. Aunque este método permite acceder a la estructura `fb_info` desde el kernel, **no refleja cambios dinámicos** en la resolución si esta es modificada por el usuario después del arranque, por ejemplo, al conectar una pantalla externa o cambiar ajustes gráficos en tiempo de ejecución.

```c
// Obtenemos fb_info directamente
    info = registered_fb[0];
    if (!info) {
        pr_err("No se encontró framebuffer 0\n");
        return -ENODEV;
    }

    // Copiamos los datos actuales
    var = info->var;

    pr_info("Resolución obtenida: %dx%d\n", var.xres, var.yres);
```

**Solución:**

- Se eliminó el uso de `registered_fb[0]` y el acceso directo a la estructura `fb_var_screeninfo`.
- Se implementó una solución basada en el subsistema DRM (Direct Rendering Manager), que ofrece una forma más robusta y moderna de obtener información de pantalla.
- La resolución se obtiene ahora mediante la iteración de conectores DRM activos (`connector_status_connected`), accediendo al modo de pantalla actual desde `drm_display_mode`.
- Se utilizaron las funciones `copy_to_user()` para retornar correctamente los valores de `width` y `height` al espacio de usuario.
- Se agregó validación de punteros mediante `copy_to_user()` (en lugar de `access_ok()`) para asegurar que las direcciones proporcionadas sean seguras y válidas.

```c
drm_file = file->private_data;
    dev = drm_file->minor->dev;

    drm_modeset_lock_all(dev);

    drm_connector_list_iter_begin(dev, &iter);
    while ((conn = drm_connector_list_iter_next(&iter))) {
        if (conn->status != connector_status_connected)
            continue;

        if (conn->state && conn->state->crtc && conn->state->crtc->state) {
            const struct drm_display_mode *m = &conn->state->crtc->state->mode;
            if (m->hdisplay && m->vdisplay) {
                w = m->hdisplay;
                h = m->vdisplay;
                found = 1;
                break;
            }
        }
    }
    drm_connector_list_iter_end(&iter);
    drm_modeset_unlock_all(dev);
    fput(file);
```

---

## ✅ Conclusiones

Modificar el kernel de Linux y agregar syscalls personalizadas es realmente una tarea compleja que requiere una comprensión profunda de su arquitectura. Los problemas encontrados fueron resueltos a través del análisis de los logs, la lectura de la documentación oficial del kernel, y una depuración manual. Esta práctica permitió un acercamiento real al funcionamiento interno del sistema operativo Linux y cómo se relaciona con el hardware y el entorno gráfico.

---

## 👨‍💻 Autor

- **Nombre**: Angel Samuel González Velásquez
- **Fecha**: Agosto 2025
- **Curso**: Laboratorio de Sistemas Operativos 2
  - **Catedrático**: Edgar René Ornelis Hoíl
  - **Auxiliar**: Steven Sullivan Jocol Gómez
