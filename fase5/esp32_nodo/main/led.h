#ifndef LED_H
#define LED_H

#include "driver/gpio.h"

typedef struct {
    gpio_num_t pin;
    int state;  // <-- agregamos el estado del LED
} Led;

void led_init(Led *led, gpio_num_t pin);
void led_on(Led *led);
void led_off(Led *led);
void led_toggle(Led *led);
int led_get_state(Led *led);

#endif
