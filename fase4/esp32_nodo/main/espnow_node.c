#include "espnow_node.h"
#include "config.h"
#include "lux_sensor.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ESPNOW_NODE";
static LuxSensor  s_lux;
static uint8_t    s_master_mac[] = MASTER_MAC;

static void sensor_task(void *arg) {
    while (1) {
        lux_sensor_read(&s_lux);
        espnow_status_t status = {
            .dark = lux_sensor_is_dark(&s_lux),
        };
        esp_now_send(s_master_mac, (uint8_t *)&status, sizeof(status));
        ESP_LOGI(TAG, "Sensor → dark:%d", status.dark);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void espnow_node_init(void) {
    lux_sensor_init(&s_lux, LUX_GPIO);

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_now_init();

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_master_mac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    ESP_LOGI(TAG, "Nodo listo — solo sensor LM393");

    xTaskCreate(sensor_task, "sensor_task", 2048, NULL, 5, NULL);
}
