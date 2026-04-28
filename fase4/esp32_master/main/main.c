#include "config.h"
#include "espnow_master.h"
#include "led.h"
#include "led_strip_ws.h"
#include "lux_sensor.h"
#include "controller.h"
#include "webserver.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include <string.h>

void lux_task(void *arg) {
    Controller *ctrl = (Controller *)arg;
    while (1) {
        if (ctrl->auto_mode) {
            bool dark_remote = g_node_status.dark;
            led_strip_ws_set_state(ctrl->strip1, dark_remote);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
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

    // Inicialización necesaria para WiFi y netif
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // ─── Solo Access Point ───
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .password = WIFI_AP_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    // Ahora ESP‑NOW
    espnow_master_init();

    // Hardware
    Led led1;
    led_init(&led1, LED1_PIN);

    LedStrip strip1;
    led_strip_ws_init(&strip1, STRIP_GPIO, STRIP_NUM_LEDS);

    LuxSensor lux1;
    lux_sensor_init(&lux1, LUX_GPIO);

    Controller controller;
    controller_init(&controller, &led1, &strip1, &lux1);

    xTaskCreate(webserver_task,    "webserver",   4096, &controller, 5, NULL);
    xTaskCreate(led_strip_ws_task, "strip_task",  4096, &strip1,     3, NULL);
    xTaskCreate(lux_task,          "lux_task",    4096, &controller, 2, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
