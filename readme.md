# esp-home 🏠

Proyecto de domótica progresiva sobre ESP32, construido fase a fase desde
cero: AP local → integración con red doméstica → MQTT + Home Assistant.

## Fases

| Fase | Descripción | Stack |
|------|-------------|-------|
| [Fase 1](./fase1/) | AP local + webserver HTTP | ESP-IDF, WiFi AP |
| [Fase 2](./fase2/) | Home Assistant en Docker | HA, Docker Compose |
| [Fase 3](./fase3/) | MQTT + fallback AP/STA | ESP-IDF, MQTT, APSTA |

Cada fase es autónoma y tiene su propio README.

## Hardware

- ESP32 (cualquier variante con WiFi)
- LED en GPIO2 (built-in en la mayoría de dev boards)

## Requisitos generales

- ESP-IDF v5.x
- Docker + Docker Compose (fase 2 en adelante)
- Broker MQTT Mosquitto (fase 3 en adelante)
