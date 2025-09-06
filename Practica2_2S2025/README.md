# 📄 Introducción al Kernel de Linux: Llamadas al Sistema

## 🧾 Descripción General

Este documento describe el proceso completo para la creación de más syscalls personalizadas, las cuales permiten realizar acciones directas relacionadas con la interacción con el entorno gráfico del sistema. Este práctica simula funcionalidades de monitoreo de pantalla útil en sistemas de escritorio remoto, herramientas de accesibilidad o sistemas de monitoreo remoto.

---

## ⚙️ Objetivos

Los objetivos de esta práctica son:

- Analizar el código fuente del kernel Linux para comprender cómo el kernel gestiona la interacción con los programas de usuario.
- Implementar nuevas llamadas al sistema utilizando lenguaje C y estructuras del kernel, con el fin de ampliar su funcionalidad y evidenciar cómo las restricciones del espacio del núcleo pueden dificultar o condicionar el comportamiento de los programas de usuario.
- Aplicar conceptos de ejecución concurrente y sincronización utilizando hilos en el kernel (cuando sea posible) para garantizar la ejecución periódica precisa de tareas.

---

## 🧩 Archivos Relevantes
