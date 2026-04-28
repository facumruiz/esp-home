#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool dark;
    uint32_t counter;       // <-- agregado
} espnow_status_t;

void espnow_node_init(void);
