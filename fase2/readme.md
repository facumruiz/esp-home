# Sistema de Domótica con ESP32 — Fase 2

## Integración con MQTT y Home Assistant (Infraestructura Docker)

## Descripción general

La Fase 2 del proyecto extiende la base funcional desarrollada en la Fase 1, incorporando capacidades de comunicación mediante MQTT y su integración con Home Assistant.

En esta etapa, el sistema evoluciona desde un enfoque exclusivamente local hacia una arquitectura conectada dentro de una red local, utilizando contenedores Docker para desplegar la infraestructura necesaria.

El objetivo es habilitar control remoto dentro de la red, visualización centralizada y una base sólida para futuras automatizaciones, manteniendo la compatibilidad con el sistema local existente.

---

## Objetivos de la Fase 2

* Integrar el ESP32 con un broker MQTT
* Desplegar el broker utilizando Docker
* Desplegar Home Assistant utilizando Docker
* Permitir la comunicación bidireccional entre ESP32 y Home Assistant
* Mantener el funcionamiento del sistema local desarrollado en la Fase 1

---

## Alcance

Esta fase incluye:

* Comunicación MQTT funcional
* Infraestructura reproducible mediante Docker Compose
* Integración con Home Assistant
* Visualización y control desde una interfaz centralizada

No incluye:

* Implementación de fallback automático
* Gestión avanzada de errores de conectividad
* Acceso remoto fuera de la red local

Estos aspectos serán abordados en la Fase 3.

---

## Arquitectura del sistema

```id="m0u7rz"
                ┌────────────────────┐
                │   Home Assistant   │
                └─────────┬──────────┘
                          │
                          │ MQTT
                          │
                ┌─────────▼──────────┐
                │   MQTT Broker      │
                │   (Mosquitto)      │
                └─────────┬──────────┘
                          │
                          │ WiFi (LAN)
                          │
                ┌─────────▼──────────┐
                │      ESP32         │
                │ (ESP-IDF + MQTT)   │
                └────────────────────┘

        Acceso adicional:
        Usuario → Web local (Fase 1)
```

---

## Infraestructura con Docker

El sistema utiliza Docker Compose para desplegar los servicios necesarios.

### Servicios incluidos

#### MQTT Broker (Mosquitto)

* Imagen: eclipse-mosquitto
* Puerto: 1883
* Persistencia de datos
* Configuración básica sin autenticación (entorno de desarrollo)

#### Home Assistant

* Imagen: homeassistant/home-assistant
* Puerto: 8123
* Persistencia de configuración
* Reinicio automático

---

## Estructura del entorno

```
docker/
├── docker-compose.yml
├── mosquitto/
│   ├── config/
│   ├── data/
│   └── log/
└── homeassistant/
    └── config/
```

---

## Puesta en marcha

### Requisitos

* Docker
* Docker Compose
* Red local compartida con el ESP32

### Ejecución

Desde el directorio `docker/`:

```
docker-compose up -d
```

---

## Acceso a servicios

* Home Assistant:

  ```
  http://localhost:8123
  ```

* Broker MQTT:

  ```
  mqtt://localhost:1883
  ```

---

## Integración MQTT en el ESP32

El ESP32 incorpora un cliente MQTT basado en `esp-mqtt`.

### Funcionalidades

* Conexión al broker en red local
* Publicación periódica de datos de sensores
* Suscripción a comandos para control de actuadores
* Reconexión básica ante desconexiones

---

## Tópicos MQTT

| Tópico             | Tipo    | Descripción              |
| ------------------ | ------- | ------------------------ |
| device/led/set     | Entrada | Control del LED (ON/OFF) |
| device/led/state   | Salida  | Estado actual del LED    |
| device/sensor/temp | Salida  | Datos de temperatura     |

---

## Integración con Home Assistant

### Configuración

Home Assistant se conecta al broker MQTT y permite:

* Visualizar datos del sensor
* Controlar el estado del LED
* Centralizar la interacción del sistema

### Entidades

* `switch`: control del LED
* `sensor`: visualización de temperatura

### Verificación

Una vez configurado:

* El ESP32 debe aparecer como dispositivo
* Se deben visualizar datos en tiempo real
* Debe ser posible encender/apagar el LED desde la interfaz

---

## Flujo de comunicación

```
ESP32 → MQTT Broker → Home Assistant
Home Assistant → MQTT Broker → ESP32

Paralelamente:
Usuario → Interfaz Web Local → ESP32
```

---

## Compatibilidad con Fase 1

El sistema mantiene completamente operativa la funcionalidad local:

* Interfaz web accesible
* Control directo del LED
* Lectura de sensores

En esta fase, la integración MQTT es complementaria y no reemplaza el control local.

---

## Estado actual del proyecto

* Infraestructura Docker operativa
* Broker MQTT funcional
* Home Assistant desplegado
* Comunicación ESP32 ↔ MQTT implementada
* Integración básica con Home Assistant

---

## Limitaciones actuales

* No se implementa fallback automático
* Dependencia manual de disponibilidad del broker
* Sin manejo avanzado de reconexión
* Sin acceso remoto fuera de la red local

---

## Próximos pasos (Fase 3)

La siguiente fase abordará:

* Implementación de fallback automático
* Gestión de estados de conectividad
* Modo híbrido (local + conectado)
* Mayor robustez ante fallos
* Posible integración con servicios externos

---

## Tecnologías utilizadas

* ESP-IDF
* FreeRTOS
* MQTT (esp-mqtt)
* Docker
* Docker Compose
* Mosquitto
* Home Assistant

---

## Conclusión

La Fase 2 introduce una capa de conectividad y control centralizado sin comprometer la operación local del sistema.

El uso de Docker permite una infraestructura reproducible y desacoplada, mientras que MQTT y Home Assistant habilitan una base sólida para la automatización y expansión del sistema en futuras fases.

