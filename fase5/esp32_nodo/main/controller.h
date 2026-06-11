#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "led.h"

typedef struct {
    Led *led1;
} Controller;

void controller_init(Controller *ctrl, Led *led1);
int controller_get_status(Controller *ctrl);

#endif
