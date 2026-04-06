#include "config.h"
#include "led.h"
#include "controller.h"
#include "webserver.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void wifi_init_softap(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();
}

void app_main(void) {
    nvs_flash_init();

    wifi_init_softap();

    Led led1;
    led_init(&led1, LED1_PIN);

    Controller controller;
    controller_init(&controller, &led1);

    xTaskCreate(webserver_task, "webserver", 4096, &controller, 5, NULL);

    // 🔥 ESTO ES CLAVE
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
