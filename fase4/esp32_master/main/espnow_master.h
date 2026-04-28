#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool dark;
    uint32_t counter;       // <-- agregado
} espnow_status_t;

extern espnow_status_t g_node_status;
void espnow_master_init(void);
