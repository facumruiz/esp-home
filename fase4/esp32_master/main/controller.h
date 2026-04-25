#pragma once
#include "led.h"
#include "led_strip_ws.h"
#include "lux_sensor.h"
#include <stdbool.h>

typedef struct {
    Led       *led1;
    LedStrip  *strip1;
    LuxSensor *lux1;
    bool       auto_mode;   // true = tira controlada por sensor
} Controller;

void controller_init(Controller *ctrl, Led *led1, LedStrip *strip1, LuxSensor *lux1);
int  controller_get_status(Controller *ctrl);
