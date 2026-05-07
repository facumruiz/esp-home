#include "pir_sensor.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "PIR";

void pir_sensor_init(PirSensor *pir, gpio_num_t pin) {
    pir->pin    = pin;
    pir->motion = false;

    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    ESP_LOGI(TAG, "PIR init OK — GPIO %d", pin);
}

void pir_sensor_read(PirSensor *pir) {
    pir->motion = (gpio_get_level(pir->pin) == 1);
}

bool pir_sensor_has_motion(PirSensor *pir) {
    return pir->motion;
}
