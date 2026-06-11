#ifndef CONFIG_H
#define CONFIG_H
#include "driver/gpio.h"

// ─── Hardware ────────────────────────────────────────────────
#define LED1_PIN       GPIO_NUM_2

// ─── Tira 1 WS2812B (control manual webserver) ───────────────
#define STRIP_GPIO      GPIO_NUM_27
#define STRIP_NUM_LEDS  8

// ─── Tira 2 WS2812B (PIR del nodo vía ESP-NOW) ───────────────
#define STRIP2_GPIO     GPIO_NUM_13
#define STRIP2_NUM_LEDS 8
#define PIR_TIMEOUT_MS  30000   // apaga tira 2 tras 30s sin movimiento

// ─── Sensor LM393 local ──────────────────────────────────────
#define LUX_GPIO GPIO_NUM_15

// ─── WiFi AP ─────────────────────────────────────────────────
#define WIFI_AP_SSID     "DomoticaLocal"
#define WIFI_AP_PASS     "12345678"

// ─── WiFi STA ────────────────────────────────────────────────
#define WIFI_STA_SSID    "Campanita"
#define WIFI_STA_PASS    "fabiana7"

// ─── MQTT ────────────────────────────────────────────────────
#define MQTT_BROKER_URI  "mqtt://192.168.100.160:1883"
#define MQTT_CLIENT_ID   "esp32-domotica"
#define MQTT_TOPIC_LED_CMD       "domotica/led1/set"
#define MQTT_TOPIC_LED_STATUS    "domotica/led1/status"
#define MQTT_TOPIC_AVAIL         "domotica/led1/avail"
#define MQTT_TOPIC_LUX_STATUS    "domotica/lux1/status"
#define MQTT_TOPIC_MOTION_STATUS "domotica/motion1/status"

// ─── Network manager ─────────────────────────────────────────
#define NM_RETRY_INTERVAL_MS    20000
#define NM_CONNECT_TIMEOUT_MS   5000

#endif // CONFIG_H

// ─── PIR timeout tira 2 ───────────────────────────────────────
#define MOTION_HOLD_MS  30000
