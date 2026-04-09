#include "controller.h"

void controller_init(Controller *ctrl, Led *led1) {
    ctrl->led1 = led1;
}

int controller_get_status(Controller *ctrl) {
    return led_get_state(ctrl->led1);
}
