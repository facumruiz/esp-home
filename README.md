# Domótica ESP32 - Fase 4

Arquitectura ESP‑NOW con dos placas.

## Master (ESP32)
- WiFi en modo AP (Access Point)
- Servidor web para control
- Tira LED WS2812B por GPIO18 (RMT)
- Sensor LM393 local de luminosidad
- Recibe datos del nodo mediante ESP‑NOW

## Nodo (ESP32)
- Sensor LM393 de luminosidad
- Envía lectura cada 2 segundos por ESP‑NOW hacia el master
- Sin tira LED ni servidor web
