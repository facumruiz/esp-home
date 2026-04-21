#pragma once
#include "led.h"
#include "led_strip_ws.h"

typedef struct {
    Led      *led1;
    LedStrip *strip1;
} Controller;

void controller_init(Controller *ctrl, Led *led1, LedStrip *strip1);
int  controller_get_status(Controller *ctrl);
