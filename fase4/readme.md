# Domótica ESP32 – Fase 4

Sistema domótico distribuido basado en ESP32 utilizando comunicación ESP-NOW entre nodos, interfaz web embebida y control automático por sensores.

## Arquitectura

El sistema está compuesto por dos placas ESP32:

- **ESP32 Master**
  - Funciona como punto de acceso WiFi (Access Point)
  - Ejecuta un servidor web embebido para monitoreo y control
  - Controla una tira LED WS2812B mediante RMT
  - Lee un sensor LM393 local de luminosidad
  - Recibe datos remotos mediante ESP-NOW
  - Publica estados vía MQTT

- **ESP32 Nodo**
  - Lee sensores remotos:
    - Sensor LM393 de luminosidad
    - Sensor PIR de movimiento
  - Envía estados al master cada 2 segundos mediante ESP-NOW
  - Funciona como nodo inalámbrico de adquisición

---

## Funcionalidades

### Control local
- Encendido y apagado de LED
- Control de brillo y color de tira WS2812B
- Interfaz web responsive

### Sensores
- Detección de luminosidad local y remota
- Detección de movimiento mediante PIR
- Visualización en tiempo real desde el panel web

### Automatización
- Modo automático basado en sensores
- Retención temporal de iluminación por movimiento (`hold`)
- Habilitación/deshabilitación dinámica del nodo remoto

### Comunicación
- ESP-NOW para comunicación entre ESP32
- MQTT para integración con sistemas externos
- HTTP para interfaz web embebida

---

## Pines de conexión
 
### ESP32 Master

| Componente | GPIO |
|---|---|
| LED integrado | GPIO2 |
| Tira WS2812B | GPIO18 |
| Sensor LM393 | GPIO15 |

#### Hardware utilizado
- ESP32
- Tira LED WS2812B
- Sensor LM393
  
---

### ESP32 Nodo
  
| Componente | GPIO |
|---|---|
| Sensor LM393 | GPIO32 |
| Sensor PIR | GPIO33 |

#### Hardware utilizado
- ESP32
- Sensor LM393
- Sensor PIR HC-SR501

---


## Comunicación entre nodos

```text
ESP32 Nodo ──ESP-NOW──► ESP32 Master ──► Web UI / MQTT
```
- Canal WiFi utilizado: 1


