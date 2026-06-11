#pragma once
#include "controller.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool     dark;
    bool     motion;
    uint32_t counter;
} espnow_status_t;

extern espnow_status_t g_node_status;

void espnow_master_init(void);
void espnow_master_init_with_ctrl(Controller *ctrl);
bool espnow_node_is_online(void);
