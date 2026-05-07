#include "controller.h"

void controller_init(Controller *ctrl, Led *led1, LedStrip *strip1, LuxSensor *lux1) {
    ctrl->led1               = led1;
    ctrl->strip1             = strip1;
    ctrl->lux1               = lux1;
    ctrl->auto_mode          = false;
    ctrl->node_enabled       = true;
    ctrl->pir_keep_on        = false;
    ctrl->pir_hold_remaining_ms = 0;
}

int controller_get_status(Controller *ctrl) {
    return led_get_state(ctrl->led1);
}
