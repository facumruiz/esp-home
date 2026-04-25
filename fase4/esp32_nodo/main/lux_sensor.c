#include "lux_sensor.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "LUX";

void lux_sensor_init(LuxSensor *lux, gpio_num_t pin) {
    lux->pin  = pin;
    lux->dark = false;

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    ESP_LOGI(TAG, "LM393 init OK — GPIO %d", pin);
}

void lux_sensor_read(LuxSensor *lux) {
    // LM393: salida DO = 0 cuando hay luz, 1 cuando oscuro
    lux->dark = (gpio_get_level(lux->pin) == 1);
}

bool lux_sensor_is_dark(LuxSensor *lux) {
    return lux->dark;
}
