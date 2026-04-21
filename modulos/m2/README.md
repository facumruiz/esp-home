# Módulo M2 — Sensor de Luminosidad LM393 + WS2812B

Extiende M1 agregando un sensor digital de luminosidad LM393 que interactúa
con la tira LED WS2812B. Incluye modo automático: la tira se enciende sola
cuando oscurece y se apaga cuando hay luz.

---

## Hardware

| Componente | Pin |
|---|---|
| LED simple (GPIO) | GPIO 2 |
| Tira WS2812B (DATA) | GPIO 18 |
| LM393 sensor (DO) | GPIO 15 |

Ajustar en `main/config.h`:

```c
#define LED1_PIN       GPIO_NUM_2
#define STRIP_GPIO     GPIO_NUM_18
#define STRIP_NUM_LEDS 30
#define LUX_GPIO       GPIO_NUM_15
```

### Conexión LM393

```
LM393 VCC  →  3.3V ESP32
LM393 GND  →  GND  ESP32
LM393 DO   →  GPIO 15       ← salida digital (la que usamos)
LM393 AO   →  no conectar   ← salida analógica, no se usa
```

El potenciómetro del módulo regula el umbral de sensibilidad.
Girarlo hasta que el LED del módulo cambie al tapar el sensor con la mano.

---

## Arquitectura

```
main/
├── main.c              # Entry point + lux_task
├── config.h            # Pines, WiFi, MQTT, timeouts
├── controller.c/h      # Struct central: Led + LedStrip + LuxSensor + auto_mode
├── led.c/h             # LED simple GPIO
├── led_strip_ws.c/h    # Tira WS2812B (RMT backend)
├── lux_sensor.c/h      # Sensor LM393 digital    ← NUEVO en M2
├── mqtt_manager.c/h    # Cliente MQTT + publish lux
├── network_manager.c/h # WiFi AP+STA con fallback
└── webserver.c/h       # Panel web + API REST
```

### Diferencias respecto a M1

| Característica | M1 | M2 |
|---|---|---|
| Sensor luminosidad | No | Si (LM393 GPIO15) |
| Modo automatico | No | Si (tira reacciona al sensor) |
| Topic MQTT lux | No | `domotica/lux1/status` |
| Endpoint `/lux/status` | No | Si |
| Endpoint `/lux/auto` | No | Si |

---

## Modo automático

Cuando el modo automático está activado, la tira responde al sensor:

```
Oscuro (DO = 1)  →  tira ON  (con el color y brillo configurados)
Hay luz (DO = 0) →  tira OFF
```

El sensor se lee cada 5 segundos. El control manual de la tira sigue
funcionando cuando el modo automático está OFF.

---

## MQTT

| Topic | Dirección | Descripción |
|---|---|---|
| `domotica/led1/set` | → ESP32 | Comando tira |
| `domotica/led1/status` | ← ESP32 | Estado LED GPIO |
| `domotica/led1/avail` | ← ESP32 | online / offline |
| `domotica/lux1/status` | ← ESP32 | DARK / LIGHT |

### Comando tira

```json
{
  "state": "ON",
  "color": { "r": 255, "g": 100, "b": 50 },
  "brightness": 128,
  "effect": "rainbow"
}
```

Efectos: `none` · `blink` · `fade` · `rainbow`

Compatibilidad legacy: `"1"` ON · `"0"` OFF

### Sensor luminosidad

```
domotica/lux1/status → "DARK"   (oscuro, DO = 1)
domotica/lux1/status → "LIGHT"  (hay luz, DO = 0)
```

Publicación cada 5 segundos con retain. Para Home Assistant:

```yaml
mqtt:
  binary_sensor:
    - name: "Luminosidad"
      state_topic: "domotica/lux1/status"
      payload_on: "DARK"
      payload_off: "LIGHT"
      device_class: light
```

---

## Panel web

Acceder desde el AP del ESP32:

```
http://192.168.4.1
```

| Seccion | Funcion |
|---|---|
| Sensor Luminosidad | Muestra estado en tiempo real (refresca cada 3s) |
| Modo automatico | Toggle ON/OFF — tira controlada por sensor |
| Tira WS2812B | ON/OFF, color picker, brillo, efectos |
| LED Simple | Toggle GPIO |

---

## API REST

| Endpoint | Metodo | Descripcion |
|---|---|---|
| `/` | GET | Panel web |
| `/status` | GET | Estado LED GPIO (`{"led1": 0/1}`) |
| `/toggle` | GET | Toggle LED GPIO |
| `/strip/status` | GET | Estado tira JSON |
| `/strip/set` | POST | Control tira (JSON) |
| `/lux/status` | GET | Estado sensor (`{"dark": bool, "auto": bool}`) |
| `/lux/auto` | GET | Toggle modo automatico |

---

## Dependencias

- ESP-IDF v5.5+
- `espressif/led_strip >= 2.5.0`
- `cJSON` (incluido en ESP-IDF)
- `espressif/mqtt`

```bash
idf.py add-dependency "espressif/led_strip>=2.5.0"
```

---

## Build y flash

```bash
idf.py build
idf.py flash monitor
```

---

## Stack de tasks FreeRTOS

| Task | Stack | Prioridad | Funcion |
|---|---|---|---|
| `webserver_task` | 4096 | 5 | HTTP server |
| `nm_task` | 4096 | 4 | WiFi + MQTT |
| `led_strip_ws_task` | 4096 | 3 | Efectos tira |
| `lux_task` | 4096 | 2 | Lectura sensor + modo auto |
