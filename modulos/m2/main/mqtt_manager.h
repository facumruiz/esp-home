#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "controller.h"
#include <stdbool.h>

// Inicia el cliente MQTT y suscribe a los topics
void mqtt_manager_start(Controller *ctrl);

// Detiene el cliente MQTT limpiamente
void mqtt_manager_stop(void);

// true si el cliente está conectado al broker
bool mqtt_manager_is_connected(void);

// Publica el estado actual del LED (llama internamente el controller)
void mqtt_manager_publish_status(Controller *ctrl);

#endif // MQTT_MANAGER_H
void mqtt_manager_publish_lux(Controller *ctrl);
