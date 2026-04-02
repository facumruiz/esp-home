# Fase 2 — Infraestructura Docker (MQTT + Home Assistant)

Este directorio contiene la infraestructura necesaria para ejecutar un entorno local de domótica basado en MQTT y Home Assistant.

## Servicios incluidos

* Mosquitto (broker MQTT)
* Home Assistant

Ambos servicios corren en contenedores Docker y permiten la integración con el ESP32 desarrollado en la Fase 1.

---

## Estructura del directorio

```
docker/
├── docker-compose.yml
├── docker-compose-rpi.yml
├── mosquitto/
│   ├── config/
│   │   └── mosquitto.conf
│   ├── data/
│   └── log/
└── homeassistant/
    └── config/
```

---

## Requisitos

* Docker instalado
* Docker Compose instalado
* Red local compartida con el ESP32

---

## Cómo levantar el entorno

### 1. Posicionarse en el directorio

```bash
cd fase2/docker
```

---

### 2. Levantar los servicios

En PC (Linux / Windows / Mac):

```bash
docker-compose up -d
```

En Raspberry Pi:

```bash
docker-compose -f docker-compose-rpi.yml up -d
```

---

### 3. Verificar contenedores

```bash
docker ps
```

Deberían estar activos:

* mqtt-broker
* home-assistant

---

## Accesos

Home Assistant:
http://localhost:8123

MQTT Broker:
Puerto 1883

---

## Integración con ESP32

Configurar en el firmware:

```c
#define MQTT_BROKER_URI "mqtt://IP_DE_TU_PC_O_RASPBERRY"
```

Importante: el ESP32 debe estar en la misma red local.

---

## Verificación básica

1. Acceder a Home Assistant
2. Configurar integración MQTT
3. Verificar entidades:

   * switch (LED)
   * sensor (temperatura)

---

## Reiniciar servicios

```bash
docker restart mqtt-broker
docker restart home-assistant
```

---

## Detener entorno

```bash
docker-compose down
```

---

## Notas importantes

* No se utiliza autenticación MQTT en esta fase (entorno de prueba)
* Los datos se persisten en volúmenes locales
* No eliminar carpetas `data/` si se desea mantener estado

---

## Troubleshooting

ESP32 no conecta a MQTT:

* Verificar IP del broker
* Verificar red local

```bash
docker logs mqtt-broker
```

---

Home Assistant no carga:

```bash
docker logs home-assistant
```

---

No aparecen entidades:

* Revisar topics MQTT
* Reiniciar Home Assistant

---

## Notas de arquitectura

Flujo de datos:

```
ESP32 → MQTT → Home Assistant
Home Assistant → MQTT → ESP32
```

Flujo local independiente:

```
Usuario → Web local → ESP32
```

---

## Próximos pasos

* Fallback automático (AP ↔ WiFi)
* MQTT Discovery
* Soporte multi-dispositivo
