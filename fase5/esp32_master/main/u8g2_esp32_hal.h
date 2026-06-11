#pragma once
#include "u8g2/u8g2.h"
#include "driver/gpio.h"

typedef struct {
    gpio_num_t sda;
    gpio_num_t scl;
} u8g2_esp32_hal_t;

#define U8G2_ESP32_HAL_DEFAULT { .sda = GPIO_NUM_21, .scl = GPIO_NUM_22 }

void u8g2_esp32_hal_init(u8g2_esp32_hal_t hal);
uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
