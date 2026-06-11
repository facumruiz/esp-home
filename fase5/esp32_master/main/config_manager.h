#pragma once
#include <stdbool.h>

// Credenciales WiFi STA
void config_set_wifi(const char *ssid, const char *password);
bool config_get_wifi(char *ssid, char *password); // false = no configurado

// Credenciales MQTT
void config_set_mqtt(const char *broker_url);
bool config_get_mqtt(char *broker_url); // false = no configurado

// Borrar configuración
void config_clear_wifi(void);
void config_clear_mqtt(void);

// Init NVS
void config_manager_init(void);
