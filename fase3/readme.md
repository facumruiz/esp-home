# Fase 3 — MQTT + Network Manager con Fallback AP/STA

El ESP32 se conecta a la red doméstica y controla el LED vía MQTT,
integrándose nativamente con Home Assistant. Si la conexión cae,
el AP local sigue activo y el webserver HTTP sigue respondiendo.

---

## Arquitectura de tareas FreeRTOS

```
app_main
├── nm_init()          — WiFi APSTA, event loop, handlers
├── webserver_task     — HTTP siempre disponible (prio 5)
└── nm_task            — FSM de red + ciclo de vida MQTT (prio 4)
```

---

## Máquina de estados — Network Manager

```
        boot
          │
     ┌────▼────┐
     │   AP    │  AP activo, sin STA
     └────┬────┘
          │ intento inmediato
     ┌────▼────────┐
     │ CONNECTING  │  esp_wifi_connect() — timeout 15 s
     └──┬──────┬───┘
        │ ok   │ fail / timeout
   ┌────▼─┐  ┌─▼────────┐
   │  STA │  │ FALLBACK │  AP activo, reintenta cada 10 s
   │      │  │          │◄─────────────────────────────┐
   └──┬───┘  └──────────┘                              │
      │ MQTT caído                                      │
      └─────────────────────────────────────────────────┘
```

En `NM_STATE_STA` el manager monitorea `mqtt_manager_is_connected()`
cada 3 s. Ante cualquier caída, destruye el cliente MQTT y transiciona
a `FALLBACK` sin perder el AP.

---

## Topics MQTT

| Topic                    | Dirección      | Payload              | Retain |
|--------------------------|----------------|----------------------|--------|
| `domotica/led1/set`      | broker → ESP32 | `1` / `0`            | no     |
| `domotica/led1/status`   | ESP32 → broker | `1` / `0`            | sí     |
| `domotica/led1/avail`    | ESP32 → broker | `online` / `offline` | sí     |

El topic `avail` con retain garantiza que Home Assistant muestre
"unavailable" si el ESP32 se desconecta sin publicar `offline`.

---

## Configuración

Editar `main/config.h` antes de compilar:

```c
// Red doméstica
#define WIFI_STA_SSID   "tu-red"
#define WIFI_STA_PASS   "tu-clave"

// Broker MQTT (IP local del host con Mosquitto / HA)
#define MQTT_BROKER_URI "mqtt://192.168.x.x:1883"

// Timeouts (ms)
#define NM_CONNECT_TIMEOUT_MS  15000   // espera de STA
#define NM_RETRY_INTERVAL_MS   10000   // pausa en FALLBACK
```

---

## Dependencias

El componente `esp-mqtt` viene incluido en ESP-IDF — no requiere
instalación externa. Solo asegurate de tener ESP-IDF v5.x:

```bash
idf.py --version
```

---

## Build y flash

```bash
cd fase3
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

> Reemplazar `/dev/ttyUSB0` por el puerto correspondiente
> (`/dev/tty.usbserial-*` en macOS, `COM*` en Windows).

---

## Integración con Home Assistant

En `configuration.yaml` (o vía UI → Dispositivos MQTT):

```yaml
mqtt:
  light:
    - name: "LED ESP32"
      unique_id: esp32_led1
      state_topic:        "domotica/led1/status"
      command_topic:      "domotica/led1/set"
      availability_topic: "domotica/led1/avail"
      payload_on:         "1"
      payload_off:        "0"
      state_on:           "1"
      state_off:          "0"
      retain: true
      optimistic: false
```

---

## Diferencias respecto a Fase 2

| Feature                  | Fase 2          | Fase 3                       |
|--------------------------|-----------------|------------------------------|
| Modo WiFi                | AP puro         | APSTA (AP + STA simultáneos) |
| Control remoto           | HTTP local      | MQTT + HTTP                  |
| Integración HA           | manual (HTTP)   | nativa MQTT con retain       |
| Disponibilidad (`avail`) | —               | retain → estado real en HA   |
| Reconexión automática    | —               | FSM con reintentos           |
| Pérdida de red           | sin control     | fallback transparente al AP  |

---

## Estructura del proyecto

```
fase3/
├── CMakeLists.txt
└── main/
    ├── config.h                        — pines, credenciales, topics, timeouts
    ├── main.c                          — arranque, tareas FreeRTOS
    ├── led.h / led.c                   — abstracción GPIO
    ├── controller.h / controller.c
    ├── webserver.h / webserver.c
    ├── network_manager.h / network_manager.c   — FSM WiFi
    └── mqtt_manager.h / mqtt_manager.c         — ciclo de vida MQTT
```
