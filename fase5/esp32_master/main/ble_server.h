#pragma once

#include "controller.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * ble_server.h
 *
 * Servidor GATT BLE para ESP-HOME.
 * Reemplaza completamente la capa webserver.h + wifi AP.
 *
 * FLUJO DE DATOS:
 *   1. ble_server_init()     → inicializa NimBLE, crea servicios/características
 *   2. ble_server_start()    → inicia advertising, listo para conexiones
 *   3. Teléfono escribe en una característica → callback modifica Controller
 *   4. ble_server_notify_*() → notifica al teléfono cambios de estado
 *
 * THREAD SAFETY:
 *   Las funciones ble_server_notify_*() son seguras para llamar desde
 *   cualquier task FreeRTOS (usan mutex interno de NimBLE).
 */

// Inicializa el stack NimBLE y registra todos los servicios GATT.
// Debe llamarse después de nvs_flash_init() y antes de ble_server_start().
void ble_server_init(Controller *ctrl);

// Inicia el advertising BLE. Llama a ble_server_init() primero.
void ble_server_start(void);

// Detiene advertising y desconecta clientes activos.
void ble_server_stop(void);

// true si hay un cliente BLE conectado actualmente.
bool ble_server_is_connected(void);

// ─── Funciones de notify activo ───────────────────────────────────────────────
// Llamar tras cualquier cambio de estado para empujar el nuevo valor al teléfono.

// Notifica estado del LED1 (uint8: 0/1)
void ble_server_notify_led1(void);

// Notifica estado completo de la tira 1 (JSON compacto)
void ble_server_notify_strip1(void);

// Notifica estado completo de la tira 2 (JSON compacto)
void ble_server_notify_strip2(void);

// Notifica estado lux + auto_mode (uint8 pack)
void ble_server_notify_lux(void);

// Notifica estado completo del nodo ESP-NOW (struct 6B)
void ble_server_notify_node(void);

// Notifica estado del PIR (uint8 pack)
void ble_server_notify_pir(void);

// Notifica resultado de operación WiFi (0=fail, 1=ok)
void ble_server_notify_wifi_result(uint8_t result);

// Notifica resultado de operación MQTT (0=fail, 1=ok)
void ble_server_notify_mqtt_result(uint8_t result);

// Notifica estado completo de red (JSON)
void ble_server_notify_net_status(void);
