#include "controller.h"

void controller_init(Controller *ctrl, Led *led1, LedStrip *strip1, LuxSensor *lux1) {
    ctrl->led1      = led1;
    ctrl->strip1    = strip1;
    ctrl->lux1      = lux1;
    ctrl->auto_mode = false;
}

int controller_get_status(Controller *ctrl) {
    return led_get_state(ctrl->led1);
}
