# 📄 Introducción al Kernel de Linux: Llamadas al Sistema

## 🧾 Descripción General

Este documento describe el proceso para la creación de syscalls personalizadas, las cuales permiten la interacción con el entorno gráfico del sistema. Este práctica simula funcionalidades de monitoreo de la pantalla, herramientas de accesibilidad o sistemas de monitoreo remoto.

---

## 🧩 Archivos Relevantes

- **Archivo del kernel (**`capture_screen.c`**):** Contiene la lógica de inicialización del dispositivo virtual y la implementación de la syscall que permite simular el movimiento del puntero del mouse.
- **Archivo del kernel (**`ipc_channel.c`**):** Contiene la lógica de inicialización del dispositivo virtual y la implementación de la syscall que simula la pulsación de una tecla.
- **Archivo del kernel (**`log_watch.c`**):** Contiene la implementación de la syscall que permite obtener la resolución actual de la pantalla principal.

---
