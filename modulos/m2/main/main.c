#include "config.h"
#include "led.h"
#include "led_strip_ws.h"
#include "lux_sensor.h"
#include "controller.h"
#include "webserver.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void lux_task(void *arg) {
    Controller *ctrl = (Controller *)arg;
    while (1) {
        lux_sensor_read(ctrl->lux1);

        if (ctrl->auto_mode) {
            // Solo seteamos estado — led_strip_ws_task aplica el cambio
            bool dark = lux_sensor_is_dark(ctrl->lux1);
            led_strip_ws_set_state(ctrl->strip1, dark);
        }

        mqtt_manager_publish_lux(ctrl);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    Led led1;
    led_init(&led1, LED1_PIN);

    LedStrip strip1;
    led_strip_ws_init(&strip1, STRIP_GPIO, STRIP_NUM_LEDS);

    LuxSensor lux1;
    lux_sensor_init(&lux1, LUX_GPIO);

    Controller controller;
    controller_init(&controller, &led1, &strip1, &lux1);

    nm_init(&controller);

    xTaskCreate(webserver_task,    "webserver",   4096, &controller, 5, NULL);
    xTaskCreate(nm_task,           "net_manager", 4096, &controller, 4, NULL);
    xTaskCreate(led_strip_ws_task, "strip_task",  4096, &strip1,     3, NULL);
    xTaskCreate(lux_task,          "lux_task",    4096, &controller, 2, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
