Actúa como un ingeniero senior en sistemas embebidos especializado en ESP32 y ESP-IDF.

Necesito que generes un proyecto completo y funcional correspondiente a la Fase 1 de un sistema de domótica local. El enfoque debe ser estrictamente offline (sin dependencia de internet) y orientado a una arquitectura escalable para futuras integraciones.

## Contexto del proyecto

El sistema forma parte de una solución de domótica modular. En esta fase inicial, el objetivo es implementar una base sólida de control local que luego permita integrar MQTT, Home Assistant y servicios en la nube en etapas posteriores.

## Requisitos funcionales

El sistema debe cumplir con lo siguiente:

* El ESP32 debe operar en modo Access Point (AP)

* Un usuario debe poder conectarse desde su celular a la red WiFi del ESP32

* El dispositivo debe exponer una interfaz web accesible en:

  http://192.168.4.1

* Desde la interfaz web se debe poder:

  * Encender y apagar un LED
  * Visualizar datos de sensores en tiempo real

## API REST

Implementar los siguientes endpoints:

* GET / → devuelve la página HTML
* GET /led/on → enciende el LED
* GET /led/off → apaga el LED
* GET /sensor → devuelve datos en formato JSON

Ejemplo de respuesta esperada:

{
"temp": 26.5
}

## Requisitos técnicos

* Utilizar exclusivamente ESP-IDF (no Arduino)
* Usar esp_http_server para el servidor HTTP
* Implementar FreeRTOS tasks para lectura de sensores (no bloquear el servidor)
* Separar el código en módulos:

  * wifi (Access Point)
  * web server (HTTP + API)
  * gpio (control de LED)
  * sensores
* Incluir una máquina de estados con al menos:

  * INIT
  * START_AP
  * START_SERVER
  * RUNNING
  * ERROR

## Frontend

* Generar una página HTML simple embebida en el firmware
* Incluir botones para controlar el LED
* Usar JavaScript para consultar periódicamente el endpoint /sensor

## Estructura del proyecto

Proveer una estructura clara de carpetas tipo:

* main/

  * main.c
  * wifi_ap.c/h
  * web_server.c/h
  * gpio_control.c/h
  * sensor.c/h
  * index_html.h

## Buenas prácticas

* Manejo de errores básico
* Código organizado y modular
* Evitar bloqueos en tareas críticas
* Uso adecuado de logs

## Requisito de producto (UX)

Incluir soporte para conexión mediante código QR:

* Generar el string estándar de WiFi:

  WIFI:T:WPA;S:<SSID>;P:<PASSWORD>;;

* Explicar cómo generar el QR

* Explicar cómo integrarlo en un producto físico (etiqueta, manual, etc.)

## Restricciones importantes

* No utilizar servicios en la nube
* No usar MQTT en esta fase
* No depender de conexión a internet
* Mantener el sistema completamente funcional en red local

## Objetivo final

El resultado debe ser un proyecto compilable en ESP-IDF, listo para ser probado en un ESP32, que funcione como base para un sistema de domótica local y escalable.

La respuesta debe ser clara, estructurada y orientada a implementación real.

