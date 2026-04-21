#include "controller.h"

void controller_init(Controller *ctrl, Led *led1, LedStrip *strip1) {
    ctrl->led1   = led1;
    ctrl->strip1 = strip1;
}

int controller_get_status(Controller *ctrl) {
    return led_get_state(ctrl->led1);
}
