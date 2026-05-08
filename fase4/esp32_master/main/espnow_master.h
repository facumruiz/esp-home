#pragma once
#include <stdbool.h>
#include <stdint.h>

#define NODE_TIMEOUT_MS  10000

typedef struct {
    bool     dark;
    bool     motion;
    uint32_t counter;
} espnow_status_t;

extern espnow_status_t g_node_status;

bool espnow_node_is_online(void);
void espnow_master_init(void);
