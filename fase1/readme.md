# Sistema de Domótica Local con ESP32 (ESP-IDF)

## Fase 1 — Control Local (Arquitectura Base)

Este proyecto corresponde a la primera fase de un sistema de domótica modular basado en ESP32.
En esta etapa se prioriza el desarrollo de un sistema completamente funcional en red local, sin dependencia de servicios externos o conectividad a internet.

El objetivo es establecer una base robusta, predecible y escalable que permita, en etapas posteriores, integrar tecnologías como MQTT, Home Assistant y acceso remoto.

---

## Objetivos de la Fase 1

* Implementar el ESP32 en modo Access Point (AP)

* Permitir la conexión directa desde dispositivos móviles

* Exponer una interfaz web accesible en:

  ```
  http://192.168.4.1
  ```

* Controlar actuadores (encendido y apagado de un LED)

* Obtener datos de sensores en tiempo real

* Definir una API REST simple

* Establecer una arquitectura modular y extensible

---

## Características principales

* Funcionamiento completamente local (offline-first)
* Interfaz web embebida en el dispositivo
* API REST para control y monitoreo
* Uso de ESP-IDF (sin dependencia de Arduino)
* Separación por módulos (wifi, servidor, gpio, sensores)
* Uso de FreeRTOS para ejecución concurrente
* Base preparada para integración futura con MQTT

---

## Arquitectura del sistema

```
Usuario (dispositivo móvil)
        ↓
WiFi (Access Point del ESP32)
        ↓
Servidor HTTP (ESP-IDF)
        ↓
┌───────────────┬───────────────┐
│ Control GPIO  │ Lectura Sensor│
└───────────────┴───────────────┘
```

---

## Máquina de estados

El sistema se estructura mediante una máquina de estados para mejorar la robustez y el control del flujo de ejecución:

* INIT: Inicialización de módulos
* START_AP: Configuración del Access Point
* START_SERVER: Inicio del servidor HTTP
* RUNNING: Operación normal
* ERROR: Manejo de fallos

---

## Endpoints disponibles

| Método | Endpoint | Descripción                    |
| ------ | -------- | ------------------------------ |
| GET    | /        | Interfaz web                   |
| GET    | /led/on  | Enciende el LED                |
| GET    | /led/off | Apaga el LED                   |
| GET    | /sensor  | Devuelve datos en formato JSON |

Ejemplo de respuesta:

```json
{
  "temp": 26.5
}
```

---

## Interfaz web

La interfaz consiste en una página HTML servida por el propio ESP32 que incluye:

* Controles para encendido y apagado del LED
* Visualización de datos del sensor
* Actualización periódica mediante JavaScript

---

## Conectividad

El dispositivo genera una red WiFi propia:

```
SSID: ESP32-DOMO
Password: 12345678
```

---

## Acceso mediante código QR

Se utiliza un código QR para simplificar la conexión del usuario al dispositivo.

Formato utilizado:

```
WIFI:T:WPA;S:ESP32-DOMO;P:12345678;;
```

Flujo de uso:

1. Escanear el código QR
2. Conectarse a la red WiFi del dispositivo
3. Acceder a la interfaz web en el navegador

---

## Estado actual del proyecto

* Control local operativo
* API REST funcional
* Interfaz web implementada
* Lectura de sensor (simulada)
* Arquitectura modular definida

---

## Próximos pasos (Fase 2)

La siguiente fase se desarrollará en paralelo con el mantenimiento del sistema local e incluirá:

### Integración con MQTT

* Implementación de un broker MQTT (por ejemplo, Mosquitto)
* Comunicación bidireccional entre ESP32 y broker
* Publicación de datos de sensores
* Suscripción para control de actuadores

### Integración con Home Assistant

* Instalación en entorno local (por ejemplo, Raspberry Pi)
* Integración mediante MQTT
* Visualización en dashboards
* Definición de automatizaciones

---

## Estrategia de desarrollo

El desarrollo del sistema se divide en dos líneas de trabajo:

* Línea principal: consolidación del sistema local (Fase 1)
* Línea paralela: preparación de infraestructura IoT (MQTT + Home Assistant)

Este enfoque permite validar el sistema base de forma independiente y reducir la complejidad inicial.

---

## Estrategia de fallback (diseño futuro)

El sistema está diseñado considerando desde el inicio un mecanismo de fallback.

En fases futuras, cuando se incorpore conectividad con servicios externos (MQTT, Home Assistant o nube), el dispositivo podrá:

* Operar en modo conectado (control remoto e integración)
* Revertir automáticamente a modo local en caso de:

  * Pérdida de conectividad
  * Fallo del broker
  * Caída de servicios externos

Esto garantiza:

* Continuidad operativa
* Independencia de la infraestructura externa
* Mayor robustez del sistema

---

## Escalabilidad futura

El diseño permite evolucionar hacia:

* Integración con plataformas de domótica
* Control remoto
* Gestión de múltiples dispositivos
* Automatizaciones avanzadas
* Actualizaciones OTA
* Mejora en seguridad y autenticación

---

## Tecnologías utilizadas

* ESP-IDF
* FreeRTOS
* esp_http_server
* WiFi en modo Access Point
* JSON para comunicación

---

## Conclusión

La Fase 1 establece una base sólida para un sistema de domótica local, priorizando confiabilidad, simplicidad y desacoplamiento de servicios externos.

Este enfoque permite construir un sistema escalable y resiliente, preparado para integrar funcionalidades avanzadas sin comprometer la operación local.

