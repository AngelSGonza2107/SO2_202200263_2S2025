#!/bin/bash

# Ruta al ejecutable de captura
CAPTURADOR="./test_capture_screen"
CARPETA="./capturas"

# Asegúrate de que la carpeta de capturas exista
mkdir -p "$CARPETA"

# Solicitar nombre del archivo al usuario
read -p "Ingresa el nombre para guardar la captura (sin extensión): " NOMBRE

# Confirmación antes de la captura
echo -n "Presiona Enter y en 5 segundos se hará una captura..."
read -r
sleep 5

# Ejecutar la captura
RUTA_SALIDA="${CARPETA}/${NOMBRE}.png"
$CAPTURADOR "$RUTA_SALIDA"

# Verificar si se generó correctamente
if [[ -f "$RUTA_SALIDA" ]]; then
    echo "Captura guardada en: $RUTA_SALIDA"
else
    echo "Error: no se pudo generar la captura."
fi
