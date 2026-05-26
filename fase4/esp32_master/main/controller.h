#pragma once
#include "led.h"
#include "led_strip_ws.h"
#include "lux_sensor.h"
#include <stdbool.h>

typedef struct {
    Led       *led1;
    LedStrip  *strip1;
    LedStrip  *strip2;
    LuxSensor *lux1;
    bool       auto_mode;
    bool       node_enabled;
    bool       pir_enabled;
    bool       pir_keep_on;
    uint32_t   pir_hold_remaining_ms;
} Controller;

void controller_init(Controller *ctrl, Led *led1, LedStrip *strip1,
                     LedStrip *strip2, LuxSensor *lux1);
int  controller_get_status(Controller *ctrl);
