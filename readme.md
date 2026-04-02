# Sistema de Domótica Modular con ESP32

## Arquitectura Local-First con Integración IoT Escalable

---

## Descripción general

Este proyecto propone el diseño e implementación de un sistema de domótica modular basado en ESP32, con una arquitectura orientada a confiabilidad, escalabilidad y desacoplamiento de servicios externos.

El enfoque adoptado es **local-first**, lo que garantiza que el sistema funcione de manera autónoma dentro de una red local, incorporando progresivamente capacidades de integración con infraestructura IoT como MQTT y Home Assistant.

El desarrollo se organiza en fases, permitiendo validar cada componente de forma incremental y reducir la complejidad del sistema.

---

## Objetivos del proyecto

* Diseñar un sistema de domótica independiente de la nube
* Permitir control local inmediato desde cualquier dispositivo
* Integrar estándares IoT ampliamente adoptados (MQTT)
* Centralizar la gestión mediante Home Assistant
* Construir una arquitectura resiliente con mecanismos de fallback
* Preparar el sistema para una futura evolución hacia producto comercial

---

## Arquitectura general

```id="p5o7tx"
                 ┌────────────────────────────┐
                 │     Home Assistant         │
                 │   (Docker / Local Server)  │
                 └────────────┬───────────────┘
                              │
                              │ MQTT
                              │
                 ┌────────────▼───────────────┐
                 │      MQTT Broker           │
                 │      (Mosquitto)           │
                 └────────────┬───────────────┘
                              │
                              │ WiFi (LAN)
                              │
        ┌─────────────────────▼─────────────────────┐
        │                  ESP32                    │
        │        (ESP-IDF + FreeRTOS)              │
        │                                          │
        │  ┌───────────────┬────────────────────┐   │
        │  │ Web Server    │ MQTT Client        │   │
        │  │ (Local UI)    │                    │   │
        │  └───────────────┴────────────────────┘   │
        │         │                │                │
        │         ▼                ▼                │
        │     Control GPIO     Sensores            │
        └──────────────────────────────────────────┘

        Acceso local directo:
        Usuario → WiFi AP → Interfaz Web
```

---

## Enfoque de desarrollo por fases

El proyecto se estructura en tres fases principales:

---

## Fase 1 — Control Local (Base del sistema)

### Objetivo

Construir un sistema completamente funcional en red local, sin dependencia de internet.

### Características

* ESP32 en modo Access Point (AP)

* Interfaz web accesible en:

  ```
  http://192.168.4.1
  ```

* API REST para control y monitoreo

* Encendido y apagado de LED

* Lectura de sensores en tiempo real

* Uso de FreeRTOS para concurrencia

* Arquitectura modular

### Resultado

Un sistema autónomo que permite:

* Control inmediato desde cualquier dispositivo
* Operación sin infraestructura externa
* Base sólida para expansión

---

## Fase 2 — Integración IoT (MQTT + Home Assistant)

### Objetivo

Incorporar conectividad y control centralizado manteniendo la funcionalidad local.

### Características

* Integración con MQTT (cliente en ESP32)

* Broker MQTT desplegado con Docker (Mosquitto)

* Home Assistant desplegado con Docker

* Comunicación bidireccional:

  ```
  ESP32 ↔ MQTT ↔ Home Assistant
  ```

* Publicación de sensores

* Control de actuadores desde interfaz centralizada

### Infraestructura

* Docker Compose para despliegue reproducible
* Red local compartida entre dispositivos

### Resultado

* Visualización centralizada
* Control remoto dentro de la red local
* Base para automatizaciones

---

## Fase 3 — Resiliencia y Modo Híbrido (Roadmap)

### Objetivo

Transformar el sistema en una solución robusta capaz de operar en múltiples escenarios de conectividad.

### Características planificadas

* Implementación de fallback automático
* Detección de estado de red y servicios
* Modo híbrido:

  * Local (AP)
  * Conectado (MQTT / Home Assistant)
* Reconexión automática a broker MQTT
* Gestión de estados del sistema

### Comportamiento esperado

* Operación normal en modo conectado
* Cambio automático a modo local ante fallos
* Continuidad operativa sin intervención del usuario

### Resultado esperado

Un sistema resiliente, tolerante a fallos y apto para entornos reales.

---

## Estrategia de fallback (visión general)

El diseño contempla desde el inicio la capacidad de fallback:

* Prioridad a operación conectada cuando está disponible
* Degradación controlada a modo local en caso de fallo
* Reintentos de conexión en segundo plano

Esto garantiza:

* Alta disponibilidad
* Independencia de infraestructura externa
* Mejor experiencia de usuario

---

## Flujo de interacción

### Control local

```id="bmnh8p"
Usuario → WiFi (AP) → ESP32 → GPIO / Sensor
```

### Control centralizado

```id="qq6r3n"
Usuario → Home Assistant → MQTT → ESP32
```

---

## Tecnologías utilizadas

### Embedded

* ESP32
* ESP-IDF
* FreeRTOS
* esp_http_server
* esp-mqtt

### Infraestructura

* Docker
* Docker Compose
* Mosquitto (MQTT Broker)
* Home Assistant

### Comunicación

* HTTP (REST)
* MQTT
* JSON

---

## Principios de diseño

* Desacoplamiento entre capas
* Modularidad
* Escalabilidad
* Resiliencia ante fallos
* Independencia de la nube
* Simplicidad operativa

---

## Casos de uso

* Automatización de iluminación
* Monitoreo ambiental
* Control local en entornos sin internet
* Integración con sistemas domóticos existentes
* Prototipado de soluciones IoT

---

## Evolución hacia producto

El proyecto está diseñado como base para una posible evolución hacia un producto comercial:

* Interfaz sin necesidad de aplicaciones externas
* Configuración simplificada mediante QR
* Arquitectura reproducible
* Integración con estándares del mercado

Futuras extensiones pueden incluir:

* Actualizaciones OTA
* Seguridad (autenticación, cifrado)
* Multi-dispositivo
* Integración con asistentes de voz
* Plataforma de gestión remota

---

## Estado del proyecto

* Fase 1: completada (control local funcional)
* Fase 2: en implementación (integración MQTT y Home Assistant)
* Fase 3: planificada (fallback y resiliencia)

---

## Conclusión

Este proyecto establece una arquitectura de domótica moderna basada en principios de robustez y escalabilidad.

El enfoque por fases permite construir un sistema confiable desde la base, integrando progresivamente capacidades avanzadas sin comprometer la operación local.

El resultado es una plataforma preparada tanto para uso práctico como para evolución hacia soluciones comerciales de mayor complejidad.

