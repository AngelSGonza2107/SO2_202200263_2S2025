# 📄 Introducción al Kernel de Linux: Llamadas al Sistema

Este documento describe el proceso para la creación de syscalls personalizadas, las cuales permiten la interacción con el entorno gráfico del sistema. Este práctica simula funcionalidades de monitoreo de la pantalla, herramientas de accesibilidad o sistemas de monitoreo remoto.

---

## 📌 Detalles Técnicos

Para esta práctica se utilizó como base el kernel de linux en su versión **linux-6.12.41**, descargada desde kernel.org, y se compiló sobre una distribución Debian-based (**Linux Mint 21.1 XFCE**). Se cambió la versión del kernel para incluir mi nombre y número de carné como identificación. Para poder descargar dichos recursos se puede acceder a esta [carpeta Drive compartida](https://uedi.ingenieria.usac.edu.gt/campus/mod/url/view.php?id=195775).

### 🧠 Funcionalidad de la Syscall **`capture_screen.c`**

```c
SYSCALL_DEFINE1(capture_screen, struct capture_struct __user *, user_c_struct)
```

- **Parámetros:**

  - **user_c_struct**: Puntero a una estructura en espacio de usuario para pasar parámetros y recibir datos capturados.

- Captura el contenido del framebuffer primario usando el subsistema DRM.
- Copia la imagen capturada en un buffer de usuario especificado.
- Devuelve metadatos como ancho, alto, bytes por fila, bytes por pixel y tamaño total de la captura.

#### 🛠️ Pasos Realizados

##### 1. **Definición de la estructura para comunicación con usuario**

Se definió la estructura `capture_struct` para que el espacio de usuario pueda:

- Proporcionar un buffer y su tamaño.
- Recibir metadatos de la captura.
- Recibir el contenido del framebuffer.

```c
struct capture_struct {
    __u64 data_pointer;      // Dirección del buffer usuario
    __u64 data_size;         // Tamaño del buffer usuario
    __u32 width;             // Ancho de la imagen capturada
    __u32 height;            // Alto de la imagen capturada
    __u32 bytes_per_row;     // Bytes por fila en la imagen
    __u32 bytes_per_pixel;   // Bytes por pixel
    __u32 total_bytes;       // Tamaño real de los datos capturados
};
```

##### 2. **Obtención del dispositivo DRM desde `/dev/fb0`**

Se implementó `get_drm_device_from_fb0()` para:

- Obtener el dispositivo DRM asociado al framebuffer principal.
- Validar que haya dispositivos framebuffer registrados.

##### 3. **Búsqueda del framebuffer primario y CRTC**

Se implementó `get_primary_fb()` para:

- Bloquear el DRM con `drm_modeset_lock_all_ctx`.
- Buscar el conector conectado y obtener su CRTC activo.
- Encontrar el plane primario asociado al CRTC.
- Extraer framebuffer, ancho y alto.

##### 4. **Captura y copia de datos**

Dentro de la syscall:

- Se copia la estructura `capture_struct` desde usuario.
- Se obtiene el dispositivo DRM y framebuffer primario.
- Se calcula el tamaño total requerido para la captura.
- Se mapea la memoria del framebuffer con `drm_gem_fb_vmap`.
- Se copia la memoria del framebuffer a un buffer temporal en kernel.
- Se copia el buffer temporal al espacio de usuario.
- Se actualizan y devuelven los metadatos.

##### 5. **Asignación del Número de Syscall**

Se definió el número de syscall como `466` en el espacio de usuario:

```c
#define SYS_CAPTURE_SCREEN 466
```

##### 6. **Modificación de Archivos del Kernel**

###### 📁 `arch/x86/entry/syscalls/syscall_64.tbl`

Agregar la entrada:

```
466     common   capture_screen     sys_capture_screen
```

###### 📁 `kernel/capture_screen.c`

Agregar la implementación completa de la syscall `capture_screen`.

##### 7. **Compilación e Instalación del Kernel**

- Recompilar kernel con `make` y `make modules_install install`.
- Actualizar gestor de arranque (`GRUB`) y reiniciar con el nuevo kernel.

#### 🧪 Prueba desde Espacio de Usuario

Se creó un programa en C para probar la syscall y guardar la captura en PNG.

##### Código de prueba: `test_capture_screen.c`

##### Compilación:

```bash
gcc test_capture_screen.c -o test_capture_screen -lpng
```

##### Ejecución:

```bash
sudo ./test_capture_screen captura.png
```

---

### 🧠 Funcionalidad de la Syscalls en **`ipc_channel.c`**

```c
SYSCALL_DEFINE2(ipc_channel_send, const char __user *, dat, u32, len)
SYSCALL_DEFINE2(ipc_channel_receive, char __user *, dat, u32, len)
```

##### `ipc_channel_send`

- **Parámetros:**

  - `dat`: Puntero al buffer en espacio de usuario con el mensaje a enviar.
  - `len`: Longitud del mensaje a enviar (máximo `512` bytes).

- Envía un mensaje a una cola compartida del kernel.
- Bloquea si la cola está llena hasta que haya espacio.
- Copia el mensaje desde espacio de usuario a una estructura en el kernel.
- Retorna la cantidad de bytes enviados o un código de error.

##### `ipc_channel_receive`

- **Parámetros:**

  - `dat`: Puntero a un buffer en espacio de usuario donde se almacenará el mensaje recibido.
  - `len`: Tamaño máximo del buffer de usuario.

- Recibe un mensaje de la cola compartida del kernel.
- Bloquea si no hay mensajes disponibles hasta que llegue uno.
- Copia el mensaje desde el kernel al espacio de usuario.
- Retorna la cantidad de bytes recibidos o un código de error.

#### 🛠️ Pasos Realizados

##### 1. Asignación del Número de Syscall

Se definieron los números en espacio de usuario:

```c
#define SYS_IPC_CHANNEL_SEND    467
#define SYS_IPC_CHANNEL_RECEIVE 468
```

##### 2. Modificación de Archivos del Kernel

###### 📁 `arch/x86/entry/syscalls/syscall_64.tbl`

Agregar las siguientes entradas:

```
467     common   ipc_channel_send      sys_ipc_channel_send
468     common   ipc_channel_receive   sys_ipc_channel_receive
```

###### 📁 `kernel/ipc_channel.c`

Agregar la implementación de ambas funciones `SYSCALL_DEFINE2`.

##### 3. Compilación e Instalación del Kernel

- Recompilar el kernel con `make` y `make modules_install install`.
- Actualizar el gestor de arranque (GRUB).
- Reiniciar el sistema con el nuevo kernel compilado.

#### 🧪 Pruebas desde Espacio de Usuario

Se implementaron dos programas simples para probar el canal IPC.

##### Código de prueba: `send.c`

###### Compilación:

```bash
gcc send.c -o ipc_send
```

###### Ejecución:

```bash
sudo ./ipc_send "Hola desde el espacio de usuario"
```

##### Código de prueba: `receive.c`

###### Compilación:

```bash
gcc receive.c -o ipc_receive
```

###### Ejecución:

```bash
sudo ./ipc_receive
```

---

### 🧠 Funcionalidad de la Syscalls en **`log_watch.c`**

```c
SYSCALL_DEFINE3(start_log_watch,
                const char __user *const __user *, paths,
                const char __user *, log_path,
                const char __user *, keyword)
SYSCALL_DEFINE1(stop_log_watch, u32, id)
```

##### `start_log_watch`

- **Parámetros:**

  - **paths**: Arreglo de hasta 5 cadenas con rutas de archivos a monitorear.
  - **log_path**: Ruta al archivo central donde se guardarán los eventos detectados.
  - **keyword**: Palabra clave que se busca en los archivos monitoreados.

- Inicia un hilo del kernel que:

  - Monitorea varios archivos de log simultáneamente.
  - Busca una palabra clave en nuevas entradas.
  - Escribe coincidencias en un archivo central de log.

- Devuelve un ID único para controlar ese monitoreo.

##### `stop_log_watch`

- **Parámetros:**

  - **id**: Identificador del monitoreo devuelto por `start_log_watch`.

- Detiene un monitoreo activo usando su ID.

- Libera todos los recursos asociados.

#### 🛠️ Pasos Realizados

##### 1. Asignación del Número de Syscall

Se definieron los números en el espacio de usuario:

```c
#define SYS_START_LOG_WATCH 469
#define SYS_STOP_LOG_WATCH  470
```

##### 2. Modificación de Archivos del Kernel

###### 📁 `arch/x86/entry/syscalls/syscall_64.tbl`

Agregar las siguientes entradas:

```
469     common   start_log_watch     sys_start_log_watch
470     common   stop_log_watch      sys_stop_log_watch
```

###### 📁 `kernel/log_watch.c`

Agregar la implementación completa de:

- `SYSCALL_DEFINE3(start_log_watch)`
- `SYSCALL_DEFINE1(stop_log_watch)`
- Todas las funciones auxiliares (`monitor_thread`, `monitor_file`, etc.)

#### 🧪 Prueba desde Espacio de Usuario

Se crearon dos programas en C para probar las syscalls:

##### Código de prueba: `start_log_watch.c`

##### Compilación:

```bash
gcc start_log_watch.c -o start_log_watch
```

##### Ejecución:

```bash
sudo ./start_log_watch /tmp/log_central.txt sudo /var/log/auth.log
```

#### Código de prueba: `stop_log_watch.c`

##### Compilación:

```bash
gcc stop_log_watch.c -o stop_log_watch
```

##### Ejecución:

```bash
sudo ./stop_log_watch 3
```

---

## ⛔ Errores encontrados

### ⚠️ Problema 1: **Fallo de segmentación al invocar la syscall desde espacio de usuario**

**Descripción:**
Al llamar a la syscall `capture_screen` desde un programa de usuario, se producía un **`segmentation fault (core dumped)`** o el programa se cerraba inesperadamente. Ytambién habían veces que provocaba una advertencia del kernel (`BUG: unable to handle kernel NULL pointer dereference`).

**Posible causa:**
El puntero `data_pointer` dentro de la estructura `capture_struct` no estaba correctamente inicializado por el programa de usuario, o apuntaba a una dirección no mapeada para escritura. Esto provocaba que el kernel intentara escribir en memoria de usuario no accesible durante la llamada a `copy_to_user`.

**Solución:**

- En el código de espacio de usuario, se aseguró que `data_pointer` apuntara a una región de memoria reservada mediante `malloc` o `mmap`, y que su tamaño fuera al menos igual a `data_size`.
- Se revisó que la estructura `capture_struct` fuera correctamente inicializada antes de hacer la syscall:

```c
struct capture_struct c = {0};
c.data_size = width * height * bytes_per_pixel;
c.data_pointer = (uintptr_t)malloc(c.data_size);
```

- Se añadió código para verificar el retorno de la syscall y mostrar errores descriptivos si `errno` era distinto de cero.

```c
if (syscall(SYS_capture_screen, &c) == -1) {
    perror("Error al capturar pantalla");
    exit(EXIT_FAILURE);
}
```

### ⚠️ Problema 2: **El framebuffer estaba vacío o la captura era completamente negra**

**Descripción:**
La syscall se ejecutaba correctamente y devolvía `0`, pero el contenido de la captura estaba completamente negro o mostraba solo un fondo sin elementos gráficos visibles, a pesar de que en pantalla sí había ventanas o contenido visible.

**Posible causa:**
La syscall accedía directamente al framebuffer primario asociado a `/dev/fb0`, pero **algunos sistemas modernos con drivers DRM/KMS no actualizan el framebuffer tradicional**, por lo que el framebuffer obtenido puede estar desactualizado o simplemente no estaba siendo usado.

**Solución:**

- Se confirmó que el framebuffer usado realmente reflejara el contenido de pantalla en tiempo real.
- Se verificó que el driver DRM utilizado soporte correctamente el framebuffer helper (`drm_fb_helper`).
- Como alternativa, se exploró capturar directamente desde el framebuffer expuesto por DRM.

```c
if (conn->status == connector_status_connected && conn->state && conn->state->crtc) {
    crtc = conn->state->crtc;
}
...
if (plane->type == DRM_PLANE_TYPE_PRIMARY && plane->state &&
    plane->state->fb && plane->state->crtc == crtc) {
    drm_framebuffer_get(plane->state->fb);
    *fb_out = plane->state->fb;
}
...
struct drm_fb_helper *helper = (struct drm_fb_helper *)info->par;
return helper ? helper->dev : NULL;
```

### ⚠️ Problema 3: **Los archivos a monitorear no se abrían correctamente en la syscall**

**Descripción:**
Al invocar la syscall `start_log_watch` desde un programa en espacio de usuario con una lista de archivos válidos para monitorear, se recibía un error como `-EBADF`, `-ENOENT` o `-EFAULT`, o simplemente no se iniciaba ningún monitoreo.

**Posible causa:**
Los paths de los archivos a monitorear eran pasados como un arreglo de punteros (`const char *const paths[]`), pero el arreglo **no estaba correctamente construido ni terminado en `NULL`**, lo que hacía que el kernel leyera direcciones inválidas al iterar.

Otra causa común era que el proceso no tenía permisos de lectura sobre los archivos, lo cual causaba que `filp_open(...)` fallara dentro de `add_watched_file(...)`.

**Solución:**

- Se aseguró que el arreglo de paths fuera correctamente inicializado y terminara con un `NULL`:

```c
const char *files[] = {
    "/var/log/syslog",
    "/var/log/auth.log",
    NULL
};
```

- Se verificó que cada archivo existiera y tuviera permisos de lectura para el usuario que invoca la syscall (`access(path, R_OK)`).
- Se añadió lógica en espacio de usuario para imprimir el error en caso de retorno negativo:

```c
int id = syscall(SYS_start_log_watch, files, log_path, "error");
if (id < 0) {
    perror("Fallo al iniciar monitoreo");
    exit(EXIT_FAILURE);
}
```

---

## 👨‍💻 Autor

- **Nombre**: Angel Samuel González Velásquez
- **Fecha**: Septiembre 2025
- **Curso**: Laboratorio de Sistemas Operativos 2
  - **Catedrático**: Edgar René Ornelis Hoíl
  - **Auxiliar**: Steven Sullivan Jocol Gómez
