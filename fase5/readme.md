# Fase 5 — Control domótico BLE

Migración completa de la comunicación local desde WiFi AP + HTTP WebServer a **Bluetooth Low Energy (BLE)** con una PWA instalable en el teléfono.

## Qué cambió respecto a fase 4

| Componente | Fase 4 | Fase 5 |
|---|---|---|
| Comunicación local | WiFi AP `DomoticaLocal` + HTTP REST | BLE GATT Server (NimBLE) |
| Interfaz de usuario | Browser conectado al AP | PWA Web Bluetooth (HTTPS) |
| Configuración de red | `config.h` hardcodeado | Configurable por BLE desde el teléfono |
| Dependencia de router | Necesaria para webserver | **Eliminada** — BLE funciona sin red |
| MQTT Home Assistant | Sí | Sí (opcional, configurable por BLE) |
| ESP-NOW nodo sensor | Sí | Sí (sin cambios) |

## Estructura

```
fase5/
├── esp32_master/          # Firmware ESP32
│   ├── main/
│   │   ├── ble_server.c/h     # Servidor GATT BLE (reemplaza webserver)
│   │   ├── ble_protocol.h     # UUIDs y constantes del protocolo
│   │   ├── main.c             # App principal (sin webserver_task)
│   │   ├── network_manager.c  # Solo WiFi STA (sin AP)
│   │   ├── espnow_master.c    # + notify BLE al recibir datos del nodo
│   │   └── ...                # Resto sin cambios desde fase 4
│   ├── partitions.csv         # Partición factory ampliada a 1500K
│   └── sdkconfig.defaults     # NimBLE habilitado, IRAM optimizado
└── pwa/                   # Interfaz Web Bluetooth
    ├── index.html             # UI completa (misma funcionalidad que fase 4)
    ├── ble-client.js          # Cliente Web Bluetooth
    ├── manifest.json          # PWA instalable
    └── sw.js                  # Service Worker (offline support)
```

## Arquitectura BLE

El ESP32 expone **4 servicios GATT**:

| Servicio | UUID | Características |
|---|---|---|
| Device Control | `0xAA01` | LED1, Strip1, Strip2 |
| Sensor Status | `0xAA02` | Lux+Auto, Node ESP-NOW, PIR |
| Network Config | `0xAA03` | WiFi, MQTT, Net Status |
| Device Info | `0x180A` | Manufacturer, Model, Firmware |

Cada característica soporta `READ + WRITE + NOTIFY`. Los estados se envían automáticamente al teléfono cuando cambian (event-driven, sin polling).

## Cómo usar

### Firmware
```bash
cd fase5/esp32_master
idf.py fullclean
idf.py build
idf.py flash
```

Requisitos: ESP-IDF v5.x con NimBLE habilitado (`idf.py menuconfig` → Component config → Bluetooth → NimBLE).

### PWA
La PWA debe servirse desde HTTPS. Opciones:

**GitHub Pages** (recomendado):
1. Subir el contenido de `pwa/` a un repositorio público
2. Activar GitHub Pages en Settings → Pages
3. Acceder desde Chrome/Edge en Android o Safari 16.4+ en iOS

**Render.com**:
- Root Directory: `fase5/pwa`
- Build Command: (vacío)
- Publish Directory: `./`

**Local** (solo para desarrollo):
```bash
cd fase5/pwa
python3 -m http.server 8080
# Abrir http://localhost:8080 en Chrome
```

### Conectar
1. Abrir la PWA en el navegador
2. Tocar **Conectar por Bluetooth**
3. Seleccionar `ESP-HOME` en el selector del sistema
4. El estado completo se sincroniza automáticamente

## Compatibilidad Web Bluetooth

| Plataforma | Soporte |
|---|---|
| Android + Chrome | ✅ Completo |
| Windows + Chrome/Edge | ✅ Completo |
| macOS + Chrome | ✅ Completo |
| iOS + Safari 16.4+ | ✅ Completo |
| Firefox | ❌ No soportado |

> Web Bluetooth requiere HTTPS (excepto `localhost`).

## Funcionalidades preservadas

- ✅ Control LED simple GPIO2
- ✅ Tira WS2812B #1 (state, color RGB, brightness, efectos blink/fade/rainbow)
- ✅ Tira WS2812B #2 (ídem)
- ✅ Sensor de luz LM393 + modo automático tira 1
- ✅ Sensor PIR vía ESP-NOW + control automático tira 2 con timeout
- ✅ Nodo ESP32 sensor (enable/disable, estado online/offline)
- ✅ Display OLED SH1106 (QR + estado del sistema)
- ✅ Configuración WiFi STA desde la app
- ✅ Configuración broker MQTT desde la app
- ✅ Integración Home Assistant vía MQTT (opcional)
- ✅ Reconexión automática WiFi y BLE

## Notas técnicas

- Stack NimBLE consume ~50KB heap adicional respecto a fase 4
- Partición factory ampliada a 1500K para acomodar BLE + WiFi + MQTT
- Optimizaciones IRAM desactivadas para reducir uso de memoria de instrucciones
- Rate limiting de 10ms entre notifys para evitar saturar el stack BLE
- Tasks de conexión WiFi/MQTT corren en prioridad 1 (mínima) para no bloquear NimBLE
