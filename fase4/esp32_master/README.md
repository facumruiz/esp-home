# Módulo M2 — Sensor LM393 + WS2812B + ESP-NOW

Extiende M1 agregando sensor de luminosidad LM393 y modo automático.
En la fase 4 este módulo fue integrado en la arquitectura multi-dispositivo
con comunicación ESP-NOW entre master y nodo.

---

## Hardware

### ESP32 Master

| Componente | GPIO |
|---|---|
| LED simple | GPIO2 |
| Tira 1 WS2812B manual | GPIO18 |
| Tira 2 WS2812B PIR | GPIO27 |
| Sensor LM393 | GPIO15 |
| OLED SDA | GPIO21 |
| OLED SCL | GPIO22 |

### ESP32 Nodo

| Componente | GPIO |
|---|---|
| Sensor LM393 | GPIO32 |
| Sensor PIR HC-SR501 | GPIO33 |

---

## Arquitectura

    main/
    espnow_master.c/h   Recepcion ESP-NOW + logica PIR tira 2
    oled_display.c/h    Pantalla OLED SH1106 QR + dashboard
    u8g2_esp32_hal.c/h  HAL I2C para u8g2
    qrcodegen.c/h       Generacion QR offline
    led_strip_ws.c/h    Tiras WS2812B RMT
    lux_sensor.c/h      Sensor LM393
    webserver.c/h       Panel web + API REST
    network_manager.c/h WiFi AP
    mqtt_manager.c/h    Cliente MQTT
    controller.c/h      Struct central con estado del sistema
    main.c              Entry point + lux_task + oled_task

---

## Modo automatico

- Tira 1: se enciende cuando el sensor de luz del nodo detecta oscuridad
- Tira 2: se enciende al detectar movimiento PIR, se apaga tras 30 segundos

---

## Pantalla OLED SH1106 1.3"

- Pantalla 1 QR: muestra QR con URL http://192.168.4.1
- Pantalla 2 Dashboard: estado de tiras, PIR, luz, modo auto y nodo
- Rota cada 10 segundos automaticamente

---

## API REST

| Endpoint | Metodo | Descripcion |
|---|---|---|
| / | GET | Panel web |
| /status | GET | Estado LED GPIO |
| /toggle | GET | Toggle LED GPIO |
| /strip1/status | GET | Estado tira 1 |
| /strip1/set | POST | Control tira 1 |
| /strip2/status | GET | Estado tira 2 |
| /strip2/set | POST | Control tira 2 |
| /lux/status | GET | Estado sensor luz |
| /lux/auto | GET | Toggle modo auto |
| /node/status | GET | Estado nodo ESP-NOW |
| /node/toggle | GET | Habilitar deshabilitar nodo |
| /pir/status | GET | Estado PIR |
| /pir/toggle | GET | Habilitar deshabilitar PIR |

---

## Stack de tasks FreeRTOS

| Task | Stack | Prioridad | Funcion |
|---|---|---|---|
| webserver_task | 4096 | 5 | HTTP server |
| nm_task | 4096 | 4 | WiFi AP |
| strip_task | 4096 | 3 | Efectos tira 1 |
| strip2_task | 4096 | 3 | Efectos tira 2 |
| oled_task | 8192 | 3 | Pantalla OLED |
| lux_task | 4096 | 2 | Sensor + modo auto |

---

## Dependencias

- ESP-IDF v5.5.1
- espressif/led_strip >= 2.5.0
- u8g2 driver OLED
- qrcodegen QR offline
- cJSON incluido en ESP-IDF

---

## Build y flash

    idf.py build
    idf.py flash monitor
