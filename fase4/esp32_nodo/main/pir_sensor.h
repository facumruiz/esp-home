#pragma once
#include "driver/gpio.h"
#include <stdbool.h>

typedef struct {
    gpio_num_t pin;
    bool       motion;
} PirSensor;

void pir_sensor_init(PirSensor *pir, gpio_num_t pin);
void pir_sensor_read(PirSensor *pir);
bool pir_sensor_has_motion(PirSensor *pir);
