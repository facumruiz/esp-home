# Domótica ESP32 — Fase 4

Sistema domótico distribuido basado en ESP32 con comunicación ESP-NOW, interfaz web embebida, pantalla OLED y control automático por sensores.

## Arquitectura

**Layered Architecture for Embedded Systems** sobre FreeRTOS con dos dispositivos ESP32:

### ESP32 Master
- Punto de acceso WiFi (AP) en 192.168.4.1
- Servidor web embebido con panel Material UI
- Dos tiras WS2812B: control manual (T1) y automático por PIR (T2)
- Pantalla OLED SH1106 1.3" con QR de conexión y dashboard de estado
- Recibe datos del nodo vía ESP-NOW

### ESP32 Nodo
- Sensor LM393 de luminosidad (GPIO32)
- Sensor PIR HC-SR501 de movimiento (GPIO33)
- Broadcast ESP-NOW al master cada 2 segundos

---

## Pines de conexión

### ESP32 Master

| Componente | GPIO |
|---|---|
| LED simple | GPIO2 |
| Tira 1 WS2812B (manual) | GPIO18 |
| Tira 2 WS2812B (PIR) | GPIO27 |
| Sensor LM393 | GPIO15 |
| OLED SDA | GPIO21 |
| OLED SCL | GPIO22 |

### ESP32 Nodo

| Componente | GPIO |
|---|---|
| Sensor LM393 | GPIO32 |
| Sensor PIR HC-SR501 | GPIO33 |

---

## Funcionalidades

### Tiras LED WS2812B
- Control manual: ON/OFF, color RGB, brillo, efectos (estático, blink, fade, rainbow)
- Tira 1: modo automático por sensor de luz del nodo
- Tira 2: se enciende al detectar movimiento PIR, timeout de 30 segundos

### Pantalla OLED SH1106 1.3"
- Pantalla 1 — QR de conexión: QR con URL http://192.168.4.1 para acceder al panel sin saber la IP
- Pantalla 2 — Dashboard: estado en tiempo real de tiras, PIR, luz, modo auto y conectividad del nodo
- Rota automáticamente cada 10 segundos

### Panel web embebido
- Accesible desde http://192.168.4.1
- Cards de control independiente para cada tira
- Switches iOS para PIR, modo auto y nodo
- Actualización en tiempo real vía polling REST

### Comunicación
- ESP-NOW: P2P entre ESP32, sin router, latencia menor a 5ms
- HTTP REST: API embebida para control y monitoreo
- AP WiFi: siempre activo, sin dependencia de red doméstica

---

## Degradación progresiva de red

| Nivel | Tecnología | Estado |
|---|---|---|
| 0 | ESP-NOW | Siempre activo |
| 1 | AP local 192.168.4.1 | Siempre activo |
| 2 | WiFi STA | No usado en esta fase |
| 3 | MQTT / Cloud | No usado en esta fase |

---

## Stack tecnológico

- ESP-IDF v5.5.1
- FreeRTOS — concurrencia por tasks con prioridades
- ESP-NOW — comunicación P2P sin router
- espressif/led_strip >= 2.5.0 — driver RMT para WS2812B
- u8g2 — driver OLED SH1106
- qrcodegen — generación QR offline
- cJSON — parseo REST
