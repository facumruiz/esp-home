#pragma once
#include "controller.h"
#include <stdbool.h>

typedef enum {
    NM_STATE_AP,
    NM_STATE_CONNECTING,
    NM_STATE_STA,
    NM_STATE_MQTT,
    NM_STATE_FALLBACK,
} nm_state_t;

void       nm_init(Controller *ctrl);
void       nm_task(void *pvParameters);
nm_state_t nm_get_state(void);
bool       nm_connect_sta(void);
bool       nm_connect_mqtt(Controller *ctrl);
void       nm_disconnect(void);
