#pragma once

#include "led_strip.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    STRIP_EFFECT_NONE    = 0,
    STRIP_EFFECT_BLINK   = 1,
    STRIP_EFFECT_FADE    = 2,
    STRIP_EFFECT_RAINBOW = 3,
} strip_effect_t;

typedef struct {
    led_strip_handle_t handle;
    uint32_t           num_leds;
    bool               state;
    uint8_t            r, g, b;
    uint8_t            brightness;
    strip_effect_t     effect;
} LedStrip;

esp_err_t led_strip_ws_init(LedStrip *strip, gpio_num_t gpio, uint32_t num_leds);
void led_strip_ws_set_state(LedStrip *strip, bool on);
void led_strip_ws_set_color(LedStrip *strip, uint8_t r, uint8_t g, uint8_t b);
void led_strip_ws_set_brightness(LedStrip *strip, uint8_t brightness);
void led_strip_ws_set_effect(LedStrip *strip, strip_effect_t effect);
void led_strip_ws_clear(LedStrip *strip);
void led_strip_ws_apply(LedStrip *strip);
void led_strip_ws_task(void *arg);
