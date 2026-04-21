# esp-home 🏠

Proyecto de domótica progresiva sobre ESP32, construido fase a fase desde
cero: AP local → integración con red doméstica → MQTT + Home Assistant.

## Fases

| Fase | Descripción | Stack |
|------|-------------|-------|
| [Fase 1](./fase1/) | AP local + webserver HTTP | ESP-IDF, WiFi AP |
| [Fase 2](./fase2/) | Home Assistant en Docker | HA, Docker Compose |
| [Fase 3](./fase3/) | MQTT + fallback AP/STA | ESP-IDF, MQTT, AP+STA |

Cada fase es autónoma y tiene su propio README.

## Módulos

Implementaciones adicionales y experimentos sobre la base de las fases.

| Módulo | Descripción | Stack |
|--------|-------------|-------|
| [M1](./modulos/m1/) | Tira WS2812B vía MQTT y webserver local | ESP-IDF, RMT, cJSON |

## Hardware

- ESP32 (cualquier variante con WiFi)
- LED en GPIO2 (built-in en la mayoría de dev boards)
- Tira WS2812B en GPIO18 (módulos)

## Requisitos generales

- ESP-IDF v5.x
- Docker + Docker Compose (fase 2 en adelante)
- Broker MQTT Mosquitto (fase 3 en adelante)
