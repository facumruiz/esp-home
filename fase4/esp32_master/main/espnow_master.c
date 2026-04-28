#include "espnow_master.h"
#include "esp_now.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ESPNOW_MASTER";

espnow_status_t g_node_status = {0};

static void on_data_recv(const esp_now_recv_info_t *info,
                          const uint8_t *data, int len) {
    if (len != sizeof(espnow_status_t)) {
        ESP_LOGW(TAG, "Tamaño incorrecto: %d vs %d", len, sizeof(espnow_status_t));
        return;
    }
    memcpy(&g_node_status, data, sizeof(espnow_status_t));
    ESP_LOGI(TAG, "Nodo → dark:%d, cnt:%lu", g_node_status.dark, (unsigned long)g_node_status.counter);
}

void espnow_master_init(void) {
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

    esp_now_peer_info_t peer = {
        .peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
        .channel = 0,
        .encrypt = false,
    };
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    ESP_LOGI(TAG, "ESP-NOW master listo (peer broadcast agregado)");
}
