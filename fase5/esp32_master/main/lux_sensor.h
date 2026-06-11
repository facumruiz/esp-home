#pragma once

#include "driver/gpio.h"
#include <stdbool.h>

typedef struct {
    gpio_num_t pin;
    bool       dark;   // true = oscuro, false = hay luz
} LuxSensor;

void lux_sensor_init(LuxSensor *lux, gpio_num_t pin);
void lux_sensor_read(LuxSensor *lux);
bool lux_sensor_is_dark(LuxSensor *lux);
