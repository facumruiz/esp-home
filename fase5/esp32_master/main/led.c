#include "led.h"
#include <stdio.h>

void led_init(Led *led, gpio_num_t pin) {
    led->pin = pin;
    led->state = 0;

    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);

    printf("[LED] GPIO %d listo y apagado\n", pin);
}

void led_on(Led *led) {
    led->state = 1;
    gpio_set_level(led->pin, 1);
}

void led_off(Led *led) {
    led->state = 0;
    gpio_set_level(led->pin, 0);
}

void led_toggle(Led *led) {
    led->state = !led->state;
    gpio_set_level(led->pin, led->state);
}

int led_get_state(Led *led) {
    return led->state;
}
