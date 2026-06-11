#pragma once

/*
 * ble_protocol.h
 *
 * Definiciones compartidas del protocolo BLE entre firmware ESP32 y PWA.
 * Toda la comunicación usa estas constantes para garantizar compatibilidad.
 *
 * ARQUITECTURA:
 *   - Servicio 1 (0xAA01): Device Control  — LED1, Strip1, Strip2
 *   - Servicio 2 (0xAA02): Sensor Status   — Lux, Node ESP-NOW, PIR
 *   - Servicio 3 (0xAA03): Network Config  — WiFi, MQTT, estado red
 *   - Servicio 4 (0x180A): Device Info     — estándar Bluetooth SIG
 *
 * PROTOCOLO DE PAQUETES:
 *   WRITE (teléfono → ESP32): [CMD 1B][LEN 1B][PAYLOAD LEN bytes]
 *   NOTIFY (ESP32 → teléfono): [TYPE 1B][SEQ 1B][DATA variable]
 *
 * MTU: El ESP32 solicita 512B. iOS puede negociar 185B.
 * Payloads > MTU-3 se fragmentan con el mecanismo de chunking.
 */

// ─── UUIDs de servicios (128-bit base: 12345678-1234-1234-1234-xxxxxxxxxxxx) ──

#define BLE_UUID_DEVICE_CONTROL_SVC  "12345678-1234-1234-1234-0000aa010000"
#define BLE_UUID_SENSOR_STATUS_SVC   "12345678-1234-1234-1234-0000aa020000"
#define BLE_UUID_NETWORK_CONFIG_SVC  "12345678-1234-1234-1234-0000aa030000"
#define BLE_UUID_DEVICE_INFO_SVC     "0000180a-0000-1000-8000-00805f9b34fb"

// ─── UUIDs de características — Servicio Device Control ───────────────────────

// LED1: uint8. 0x00=OFF, 0x01=ON. Notify al cambiar.
#define BLE_UUID_CHAR_LED1           "12345678-1234-1234-1234-0001aa010000"

// Strip1 state: JSON compacto ≤ MTU. Notify al cambiar.
// Write payload: {"s":1,"r":255,"g":100,"b":0,"br":128,"ef":0}
// s=state, r/g/b=color, br=brightness, ef=effect
#define BLE_UUID_CHAR_STRIP1         "12345678-1234-1234-1234-0002aa010000"

// Strip2 state: mismo formato que Strip1.
#define BLE_UUID_CHAR_STRIP2         "12345678-1234-1234-1234-0003aa010000"

// ─── UUIDs de características — Servicio Sensor Status ────────────────────────

// Lux + Auto: uint8 pack. Bit0=dark, Bit1=auto_mode.
// Write: Bit1 para toggle auto_mode. Notify al cambiar.
#define BLE_UUID_CHAR_LUX            "12345678-1234-1234-1234-0001aa020000"

// Node status: 6 bytes struct.
// [enabled:1][online:1][dark:1][motion:1][pir_enabled:1][hold_ms:2] (little-endian)
// Notify automático cada vez que el nodo envía por ESP-NOW.
#define BLE_UUID_CHAR_NODE           "12345678-1234-1234-1234-0002aa020000"

// PIR control: uint8. Bit0=pir_enabled, Bit1=motion, Bit2=keep_on.
// Write: 0x01 para toggle pir_enabled.
#define BLE_UUID_CHAR_PIR            "12345678-1234-1234-1234-0003aa020000"

// ─── UUIDs de características — Servicio Network Config ───────────────────────

// WiFi credentials: Write SSID\0PASS (null-separated).
// Notify result: 0x00=failed, 0x01=connected.
// Write 0xFF para clear.
#define BLE_UUID_CHAR_WIFI           "12345678-1234-1234-1234-0001aa030000"

// MQTT broker: Write URL string.
// Notify result: 0x00=failed, 0x01=connected.
// Write 0xFF para clear.
#define BLE_UUID_CHAR_MQTT           "12345678-1234-1234-1234-0002aa030000"

// Network status: Read/Notify JSON compacto.
// {"wc":1,"ws":"MiRed","mc":0,"mb":""}
// wc=wifi_connected, ws=wifi_ssid, mc=mqtt_connected, mb=mqtt_broker
#define BLE_UUID_CHAR_NET_STATUS     "12345678-1234-1234-1234-0003aa030000"

// ─── UUIDs de características — Device Info (estándar BT SIG) ────────────────

#define BLE_UUID_CHAR_MANUFACTURER   "00002a29-0000-1000-8000-00805f9b34fb"
#define BLE_UUID_CHAR_MODEL          "00002a24-0000-1000-8000-00805f9b34fb"
#define BLE_UUID_CHAR_FIRMWARE_REV   "00002a26-0000-1000-8000-00805f9b34fb"

// ─── Comandos WRITE para características que los necesitan ────────────────────

// LED1: valor directo uint8
#define BLE_CMD_LED_OFF              0x00
#define BLE_CMD_LED_ON               0x01
#define BLE_CMD_LED_TOGGLE           0x02

// Lux/Auto: máscara de bits en write
#define BLE_CMD_AUTO_TOGGLE          0x02  // Bit1 = toggle auto_mode

// Node: commands
#define BLE_CMD_NODE_TOGGLE          0x01

// PIR: commands
#define BLE_CMD_PIR_TOGGLE           0x01

// WiFi/MQTT: comando especial para clear
#define BLE_CMD_CLEAR                0xFF

// ─── Nombres del dispositivo BLE ──────────────────────────────────────────────

#define BLE_DEVICE_NAME              "ESP-HOME"
#define BLE_MANUFACTURER_NAME        "ESP-Domotica"
#define BLE_MODEL_NUMBER             "ESP32-Master-v4"
#define BLE_FIRMWARE_REVISION        "4.0.0-ble"

// ─── Constantes de comportamiento BLE ────────────────────────────────────────

// Intervalo de advertising (ms). 100ms es buen balance velocidad/energía.
#define BLE_ADV_INTERVAL_MS          100

// Notify debounce para sensores que cambian rápido (ms)
#define BLE_NOTIFY_DEBOUNCE_LUX_MS   2000
#define BLE_NOTIFY_DEBOUNCE_PIR_MS   500

// Timeout para considerar cliente desconectado y reiniciar advertising (ms)
#define BLE_CLIENT_TIMEOUT_MS        30000
