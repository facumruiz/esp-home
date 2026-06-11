#pragma once
#include "driver/gpio.h"

// ─── Hardware ────────────────────────────────────────────────
#define LED1_PIN       GPIO_NUM_2
#define STRIP_GPIO     GPIO_NUM_18
#define STRIP_NUM_LEDS 8
#define LUX_GPIO       GPIO_NUM_32
#define PIR_GPIO       GPIO_NUM_33

// ─── ESP-NOW ─────────────────────────────────────────────────
#define MASTER_MAC {0xE0, 0x5A, 0x1B, 0xD0, 0x6F, 0xE8}
