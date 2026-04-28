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
static uint8_t    broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static void sensor_task(void *arg) {
    uint32_t counter = 0;
    while (1) {
        lux_sensor_read(&s_lux);
        espnow_status_t status = {
            .dark = lux_sensor_is_dark(&s_lux),
            .counter = counter++,
        };
        esp_err_t err = esp_now_send(broadcast_mac, (uint8_t *)&status, sizeof(status));
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Enviado: %s (cnt=%lu)", status.dark ? "OSCURO" : "HAY LUZ", (unsigned long)status.counter);
        } else {
            ESP_LOGE(TAG, "Error al enviar: %s", esp_err_to_name(err));
        }
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

    // Configurar STA vacía para que no se conecte a ninguna red
    wifi_config_t sta_config = { .sta = { .ssid = "", .password = "" } };
    esp_wifi_set_config(WIFI_IF_STA, &sta_config);

    esp_wifi_start();
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    esp_now_init();

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, broadcast_mac, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    ESP_LOGI(TAG, "Nodo listo — enviando broadcast cada 2s");

    xTaskCreate(sensor_task, "sensor_task", 2048, NULL, 5, NULL);
}
