#pragma once
#include <stdbool.h>

typedef struct {
    bool dark;
} espnow_status_t;

extern espnow_status_t g_node_status;

void espnow_master_init(void);
