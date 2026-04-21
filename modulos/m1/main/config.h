#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"

// ─── Hardware ────────────────────────────────────────────────
#define LED1_PIN GPIO_NUM_2

// ─── WiFi AP (modo local, igual que Fase 1) ──────────────────
#define WIFI_AP_SSID     "DomoticaLocal"
#define WIFI_AP_PASS     "12345678"

// ─── WiFi STA (tu red doméstica) ─────────────────────────────
#define WIFI_STA_SSID    "Campanita"
#define WIFI_STA_PASS    "fabiana7"   // <-- completar

// ─── MQTT ────────────────────────────────────────────────────
#define MQTT_BROKER_URI  "mqtt://192.168.100.160:1883"
#define MQTT_CLIENT_ID   "esp32-domotica"

#define MQTT_TOPIC_LED_CMD    "domotica/led1/set"      // recibe: "1" / "0"
#define MQTT_TOPIC_LED_STATUS "domotica/led1/status"   // publica: "1" / "0"

#define MQTT_TOPIC_AVAIL      "domotica/led1/avail"


// ─── Network manager ─────────────────────────────────────────
#define NM_RETRY_INTERVAL_MS   10000   // reintento cada 10s en FALLBACK
#define NM_CONNECT_TIMEOUT_MS  15000   // timeout para CONNECTING

#endif // CONFIG_H

// ── WS2812B ──────────────────────────────────────────────────────────────────
#define STRIP_GPIO      GPIO_NUM_18   // ← cambiá al pin real
#define STRIP_NUM_LEDS  30           // ← cambiá a tu cantidad de LEDs
