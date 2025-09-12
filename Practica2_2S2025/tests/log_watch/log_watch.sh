#!/bin/bash

LOG_DIR="./temp"
mkdir -p "$LOG_DIR"

LOG_FILES=()
LOG_WATCH_ID=""

crear_logs() {
    echo -n "¿Cuántos archivos deseas crear? (1-5): "
    read -r count

    if ! [[ "$count" =~ ^[1-5]$ ]]; then
        echo "Cantidad inválida. Debe ser entre 1 y 5."
        return
    fi

    LOG_FILES=()
    for ((i=1; i<=count; i++)); do
        file="$LOG_DIR/log$i.log"
        touch "$file"
        LOG_FILES+=("$file")
        echo "Archivo creado: $file"
    done
}

iniciar_monitoreo() {
    if [ ${#LOG_FILES[@]} -eq 0 ]; then
        echo "Primero debes crear archivos de log (Opción 1)."
        return
    fi

    echo -n "Ingresa el nombre del archivo central de log: "
    read -r central_log_name
    CENTRAL_LOG="$LOG_DIR/$central_log_name.log"
    touch "$CENTRAL_LOG"

    echo -n "Ingresa la palabra o palabras clave a monitorear: "
    read -r keyword

    echo "Iniciando monitoreo..."

    output=$(sudo ./test_start_log_watch "$CENTRAL_LOG" "$keyword" "${LOG_FILES[@]}")
    status=$?

    if [ $status -ne 0 ]; then
        echo "Error al iniciar monitoreo:"
        echo "$output"
        return
    fi

    # Extraer el ID con una expresión regular
    LOG_WATCH_ID=$(echo "$output" | grep -oE '[0-9]+')
    echo "Monitoreo iniciado con ID = $LOG_WATCH_ID"
}

simular_log() {
    if [ ${#LOG_FILES[@]} -eq 0 ]; then
        echo "Primero debes crear archivos de log (Opción 1)."
        return
    fi

    echo "¿Qué mensaje quieres agregar a los logs?"
    read -r mensaje

    echo "¿A cuál log deseas agregarlo? (1-${#LOG_FILES[@]})"
    for i in "${!LOG_FILES[@]}"; do
        echo "$((i+1)). ${LOG_FILES[$i]}"
    done
    read -r log_index

    if ! [[ "$log_index" =~ ^[1-${#LOG_FILES[@]}]$ ]]; then
        echo "Selección inválida."
        return
    fi

    echo "$mensaje" >> "${LOG_FILES[$((log_index-1))]}"
    echo "Mensaje agregado."
}

ver_log_central() {
    echo "Archivos disponibles en $LOG_DIR:"
    ls "$LOG_DIR"
    echo "¿Qué archivo central deseas ver?"
    read -r archivo
    archivo="$LOG_DIR/$archivo"

    if [ ! -f "$archivo" ]; then
        echo "Archivo no encontrado."
        return
    fi

    echo "Contenido de $archivo:"
    echo "--------------------------------------"
    cat "$archivo"
    echo "--------------------------------------"
}

detener_monitoreo() {
    echo "Ingresa el ID del monitoreo a detener:"
    read -r id

    sudo ./test_stop_log_watch "$id"
    if [ $? -ne 0 ]; then
        echo "Error al detener monitoreo con ID $id"
    else
        echo "Monitoreo detenido correctamente."
    fi
}

while true; do
    echo ""
    echo "===== MENÚ DE LOG MONITOREO ====="
    echo "1. Crear archivos de log"
    echo "2. Iniciar monitoreo"
    echo "3. Simular nuevo log (append)"
    echo "4. Ver log central"
    echo "5. Detener monitoreo"
    echo "6. Salir"
    echo -n "Selecciona una opción: "
    read -r opcion

    case "$opcion" in
        1) crear_logs ;;
        2) iniciar_monitoreo ;;
        3) simular_log ;;
        4) ver_log_central ;;
        5) detener_monitoreo ;;
        6) echo "Saliendo..."; break ;;
        *) echo "Opción inválida." ;;
    esac
done
