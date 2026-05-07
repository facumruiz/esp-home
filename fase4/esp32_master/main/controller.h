#pragma once
#include "led.h"
#include "led_strip_ws.h"
#include "lux_sensor.h"
#include <stdbool.h>

#define MOTION_HOLD_MS  15000

typedef struct {
    Led       *led1;
    LedStrip  *strip1;
    LuxSensor *lux1;
    bool       auto_mode;
    bool       node_enabled;
    bool       pir_keep_on;      // true = LED encendido por PIR (activo o hold)
    uint32_t   pir_hold_remaining_ms; // ms restantes del hold
} Controller;

void controller_init(Controller *ctrl, Led *led1, LedStrip *strip1, LuxSensor *lux1);
int  controller_get_status(Controller *ctrl);
