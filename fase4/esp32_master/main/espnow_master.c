#include "espnow_master.h"
#include "esp_now.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ESPNOW_MASTER";

espnow_status_t  g_node_status   = {0};
static TickType_t s_last_recv_tick = 0;

static void on_data_recv(const esp_now_recv_info_t *info,
                          const uint8_t *data, int len) {
    if (len != sizeof(espnow_status_t)) {
        ESP_LOGW(TAG, "Tamanio incorrecto: %d vs %d", len, (int)sizeof(espnow_status_t));
        return;
    }
    memcpy(&g_node_status, data, sizeof(espnow_status_t));
    s_last_recv_tick = xTaskGetTickCount();
    ESP_LOGI(TAG, "Nodo -> dark:%d motion:%d cnt:%lu",
             g_node_status.dark,
             g_node_status.motion,
             (unsigned long)g_node_status.counter);
}

bool espnow_node_is_online(void) {
    if (s_last_recv_tick == 0) return false;
    uint32_t elapsed = (xTaskGetTickCount() - s_last_recv_tick) * portTICK_PERIOD_MS;
    return elapsed < NODE_TIMEOUT_MS;
}

void espnow_master_init(void) {
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));
    esp_now_peer_info_t peer = {
        .peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
        .channel   = 0,
        .encrypt   = false,
    };
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    ESP_LOGI(TAG, "ESP-NOW master listo");
}
