Actúa como un ingeniero senior en sistemas IoT especializado en ESP32 (ESP-IDF), MQTT y Home Assistant, con experiencia en despliegues con Docker.

Necesito que generes el diseño e implementación correspondiente a la **Fase 2** de un sistema de domótica modular previamente desarrollado. En esta fase el foco está exclusivamente en la integración con MQTT y Home Assistant utilizando Docker, sin implementar aún lógica de fallback.

## Contexto del proyecto

En la Fase 1 ya existe:

* Un ESP32 funcionando en modo Access Point (AP)
* Interfaz web local en http://192.168.4.1
* API REST para control de LED y lectura de sensores
* Arquitectura modular con FreeRTOS
* Sistema completamente funcional sin internet

La Fase 2 debe agregar conectividad con infraestructura local sin romper el funcionamiento existente.

---

## Objetivos de la Fase 2

* Integrar comunicación MQTT en el ESP32
* Desplegar un broker MQTT usando Docker
* Desplegar Home Assistant usando Docker
* Permitir la integración entre ESP32 y Home Assistant
* Mantener compatibilidad total con la Fase 1

---

## Alcance de esta fase

Importante:

* No implementar fallback en esta fase
* No modificar la lógica base de operación local
* No eliminar el modo Access Point existente
* El sistema local debe seguir funcionando independientemente

El fallback será implementado en la Fase 3.

---

## Infraestructura (Docker)

Se debe definir un entorno reproducible utilizando Docker Compose.

### Requisitos:

Crear un archivo `docker-compose.yml` que incluya:

### Servicio MQTT (Mosquitto)

* Imagen: eclipse-mosquitto
* Puerto:

  * 1883 (MQTT)
* Persistencia de datos
* Configuración básica funcional (sin autenticación para pruebas iniciales)

### Servicio Home Assistant

* Imagen: homeassistant/home-assistant
* Puerto:

  * 8123
* Volumen persistente
* Reinicio automático

---

## Estructura esperada

* docker/

  * docker-compose.yml
  * mosquitto/

    * config/
    * data/
    * log/
  * homeassistant/

    * config/

---

## MQTT en el ESP32

Implementar cliente MQTT usando `esp-mqtt`.

### Funcionalidades requeridas:

* Conexión al broker (IP local del host donde corre Docker)
* Publicación periódica de datos de sensores
* Suscripción a comandos para control de actuadores
* Manejo básico de reconexión

### Tópicos sugeridos:

* device/led/set        (entrada de comandos)
* device/led/state      (estado del LED)
* device/sensor/temp    (datos de sensor)

---

## Integración con Home Assistant

### Requisitos:

* Integración mediante MQTT
* Configuración manual o mediante MQTT Discovery

### Entidades mínimas:

* Un switch para controlar el LED
* Un sensor para visualizar la temperatura

### Incluir:

* Ejemplo de configuración en Home Assistant
* Cómo verificar la recepción de datos
* Cómo controlar el dispositivo desde la interfaz

---

## Arquitectura del sistema

Describir el flujo:

ESP32 → MQTT Broker → Home Assistant
Home Assistant → MQTT Broker → ESP32

Mantener también:

Usuario → Web local → ESP32

---

## Buenas prácticas

* No bloquear tareas en el ESP32
* Manejo de errores en conexión MQTT
* Logs claros
* Código modular y desacoplado

---

## Entorno de pruebas

### Requisitos:

* PC o Raspberry Pi con Docker instalado
* Red local compartida con el ESP32

### Pasos esperados:

1. Levantar servicios:

   ```
   docker-compose up -d
   ```

2. Acceder a Home Assistant:

   ```
   http://localhost:8123
   ```

3. Configurar integración MQTT en Home Assistant

4. Verificar:

   * Recepción de datos del ESP32
   * Control del LED desde Home Assistant

---

## Compatibilidad con Fase 1

El sistema debe seguir funcionando en modo local si:

* El broker no está disponible
* Home Assistant no está corriendo
* No hay red externa

Sin embargo, en esta fase no es necesario automatizar el cambio de modos (fallback).

---

## Objetivo final

El resultado debe ser un sistema funcional donde:

* El ESP32 se comunica con un broker MQTT en Docker
* Home Assistant consume esos datos y permite control
* El sistema local sigue siendo operativo
* La base queda preparada para implementar fallback en la Fase 3

La respuesta debe ser clara, estructurada y orientada a implementación real.

