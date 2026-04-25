#pragma once
#include <stdbool.h>

typedef struct {
    bool dark;
} espnow_status_t;

void espnow_node_init(void);
