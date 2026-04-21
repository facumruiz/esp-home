#include "config.h"
#include "led.h"
#include "led_strip_ws.h"
#include "controller.h"
#include "webserver.h"
#include "network_manager.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

    Controller controller;
    controller_init(&controller, &led1, &strip1);

    nm_init(&controller);

    xTaskCreate(webserver_task,    "webserver",   4096, &controller, 5, NULL);
    xTaskCreate(nm_task,           "net_manager", 4096, &controller, 4, NULL);
    xTaskCreate(led_strip_ws_task, "strip_task",  4096, &strip1,     3, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
