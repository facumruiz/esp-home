#include "config.h"
#include "led.h"
#include "controller.h"
#include "webserver.h"
#include "network_manager.h"

#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    // NVS obligatorio para WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Hardware
    Led led1;
    led_init(&led1, LED1_PIN);

    Controller controller;
    controller_init(&controller, &led1);

    // Red: AP + STA + fallback automático
    // (crea el event loop y arranca WiFi en modo APSTA)
    nm_init(&controller);

    // Webserver: siempre disponible (AP o STA)
    xTaskCreate(webserver_task, "webserver", 4096, &controller, 5, NULL);

    // Network manager: máquina de estados + MQTT
    xTaskCreate(nm_task, "net_manager", 4096, &controller, 4, NULL);

    // Loop principal
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
