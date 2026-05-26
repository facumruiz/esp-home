#include "espnow_master.h"
#include "config.h"
#include "led_strip_ws.h"
#include "esp_now.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

#define NODE_TIMEOUT_MS 10000

static const char *TAG       = "ESPNOW_MASTER";
static Controller *s_ctrl    = NULL;
static int64_t     s_last_rx_us     = 0;
static int64_t     s_last_motion_us = 0;

espnow_status_t g_node_status = {0};

bool espnow_node_is_online(void) {
    if (s_last_rx_us == 0) return false;
    int64_t elapsed = (esp_timer_get_time() - s_last_rx_us) / 1000;
    return elapsed < NODE_TIMEOUT_MS;
}

static void on_data_recv(const esp_now_recv_info_t *info,
                          const uint8_t *data, int len) {
    if (len != sizeof(espnow_status_t)) return;
    memcpy(&g_node_status, data, sizeof(espnow_status_t));
    s_last_rx_us = esp_timer_get_time();

    ESP_LOGI(TAG, "Nodo → dark:%d motion:%d cnt:%lu",
        g_node_status.dark,
        g_node_status.motion,
        (unsigned long)g_node_status.counter);

    if (!s_ctrl || !s_ctrl->strip2 || !s_ctrl->pir_enabled || !s_ctrl->node_enabled) return;

    if (g_node_status.motion) {
        s_last_motion_us = esp_timer_get_time();
        s_ctrl->pir_keep_on          = true;
        s_ctrl->pir_hold_remaining_ms = MOTION_HOLD_MS;
        led_strip_ws_set_state(s_ctrl->strip2, true);
        led_strip_ws_set_color(s_ctrl->strip2, 255, 200, 100);
        led_strip_ws_set_brightness(s_ctrl->strip2, 200);
        led_strip_ws_apply(s_ctrl->strip2);
        ESP_LOGI(TAG, "PIR: movimiento → tira 2 ON");
    } else {
        int64_t elapsed_ms = (esp_timer_get_time() - s_last_motion_us) / 1000;
        int64_t remaining  = MOTION_HOLD_MS - elapsed_ms;
        if (remaining > 0) {
            s_ctrl->pir_keep_on           = true;
            s_ctrl->pir_hold_remaining_ms = (uint32_t)remaining;
        } else {
            s_ctrl->pir_keep_on           = false;
            s_ctrl->pir_hold_remaining_ms = 0;
            led_strip_ws_set_state(s_ctrl->strip2, false);
            led_strip_ws_apply(s_ctrl->strip2);
            ESP_LOGI(TAG, "PIR: timeout → tira 2 OFF");
        }
    }
}

void espnow_master_init_with_ctrl(Controller *ctrl) {
    s_ctrl = ctrl;
    esp_now_init();
    esp_now_register_recv_cb(on_data_recv);
    ESP_LOGI(TAG, "ESP-NOW master listo");
}

void espnow_master_init(void) {
    espnow_master_init_with_ctrl(NULL);
}
