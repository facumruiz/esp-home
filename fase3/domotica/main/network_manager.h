#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "controller.h"

// Estados de la máquina de red
typedef enum {
    NM_STATE_AP,          // Access Point puro (Fase 1)
    NM_STATE_CONNECTING,  // Intentando STA + MQTT
    NM_STATE_STA,         // Conectado: WiFi + MQTT activos
    NM_STATE_FALLBACK,    // STA/MQTT caído, AP activo, reintentando en BG
} nm_state_t;

// Inicializa el network manager (llama a wifi_init_ap internamente)
void nm_init(Controller *ctrl);

// Tarea FreeRTOS principal — pasar como pvParameters el Controller*
void nm_task(void *pvParameters);

// Estado actual (thread-safe, lectura atómica)
nm_state_t nm_get_state(void);

#endif // NETWORK_MANAGER_H
