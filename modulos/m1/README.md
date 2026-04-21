# Módulo M1 — Control WS2812B vía MQTT y Webserver

Control de tira LED direccionable WS2812B sobre ESP32 con ESP-IDF, integrado a red domótica vía MQTT y panel web local.

---

## Hardware

| Componente | Pin |
|---|---|
| LED simple (GPIO) | GPIO 2 |
| Tira WS2812B (DATA) | GPIO 18 |

Ajustar en `main/config.h`:

```c
#define LED1_PIN       GPIO_NUM_2
#define STRIP_GPIO     GPIO_NUM_18
#define STRIP_NUM_LEDS 30
```

---

## Arquitectura

```
main/
├── main.c              # Entry point, init de periféricos y tasks
├── config.h            # Pines, credenciales WiFi, broker MQTT
├── controller.c/h      # Struct central que agrupa Led + LedStrip
├── led.c/h             # Control LED simple por GPIO
├── led_strip_ws.c/h    # Control tira WS2812B (RMT backend)
├── mqtt_manager.c/h    # Cliente MQTT, parseo JSON, comandos
├── network_manager.c/h # WiFi AP+STA con fallback automático
└── webserver.c/h       # HTTP server con panel web + API REST
```

### Módulos principales

**`led_strip_ws`** — Wrapper sobre el componente oficial `espressif/led_strip`:
- Init RMT, set color RGB, brillo (0-255), efectos
- Task FreeRTOS independiente para efectos animados

**`network_manager`** — Modo dual AP+STA:
- Intenta conectar a la red STA configurada
- Si falla, hace fallback a AP (siempre activo)
- Reintentos automáticos cada 10 segundos

**`mqtt_manager`** — Cliente MQTT con compatibilidad dual:
- Acepta comandos legacy `"0"` / `"1"`
- Acepta JSON completo con state, color, brightness y effect

**`webserver`** — Panel web accesible desde el AP:
- UI con color picker, slider de brillo y botones de efecto
- API REST en `/strip/set` (POST JSON) y `/strip/status` (GET)

---

## MQTT

| Topic | Dirección | Descripción |
|---|---|---|
| `domotica/led1/set` | → ESP32 | Comando |
| `domotica/led1/status` | ← ESP32 | Estado actual |
| `domotica/led1/avail` | ← ESP32 | online / offline |

### Formato de comando

```json
{
  "state": "ON",
  "color": { "r": 255, "g": 100, "b": 50 },
  "brightness": 128,
  "effect": "rainbow"
}
```

Efectos disponibles: `none` · `blink` · `fade` · `rainbow`

Compatibilidad legacy:

```
"1"  →  ON
"0"  →  OFF
```

---

## Panel web

Acceder desde cualquier dispositivo conectado al AP del ESP32:

```
http://192.168.4.1
```

Controles disponibles:
- Toggle LED GPIO
- ON / OFF tira
- Color picker RGB
- Slider de brillo
- Botones de efecto

---

## Dependencias

- ESP-IDF v5.5+
- `espressif/led_strip >= 2.5.0` (managed component)
- `cJSON` (incluido en ESP-IDF)
- `espressif/mqtt` (managed component)

Instalar dependencias:

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

| Task | Stack | Prioridad |
|---|---|---|
| `webserver_task` | 4096 | 5 |
| `nm_task` | 4096 | 4 |
| `strip_task` | 4096 | 3 |
