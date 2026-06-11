#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool     dark;
    bool     motion;
    uint32_t counter;
} espnow_status_t;

void espnow_node_init(void);
