#include "network_manager.h"
#include "config_manager.h"
#include "mqtt_manager.h"
#include "config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "NM";
static nm_state_t s_state = NM_STATE_AP;
static EventGroupHandle_t s_wifi_eg;
static esp_netif_t *s_netif_sta = NULL;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "STA desconectado");
        xEventGroupSetBits(s_wifi_eg, WIFI_FAIL_BIT);
    }
    if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP obtenida: " IPSTR, IP2STR(&ev->ip_info.ip));
        xEventGroupSetBits(s_wifi_eg, WIFI_CONNECTED_BIT);
    }
}

void nm_init(Controller *ctrl) {
    (void)ctrl;
    s_wifi_eg = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();
    s_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        wifi_event_handler, NULL, NULL);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid           = WIFI_AP_SSID,
            .ssid_len       = strlen(WIFI_AP_SSID),
            .password       = WIFI_AP_PASS,
            .max_connection = 4,
            .authmode       = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "AP activo: %s — 192.168.4.1", WIFI_AP_SSID);
}

// Llamado desde webserver cuando se guarda config WiFi
bool nm_connect_sta(void) {
    char ssid[64] = {0};
    char pass[64] = {0};
    if (!config_get_wifi(ssid, pass)) {
        ESP_LOGW(TAG, "Sin credenciales WiFi guardadas");
        return false;
    }

    ESP_LOGI(TAG, "Conectando a STA: %s", ssid);
    s_state = NM_STATE_CONNECTING;

    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid,     ssid, 31);
    strncpy((char *)sta_cfg.sta.password, pass, 63);
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(s_wifi_eg,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & WIFI_CONNECTED_BIT) {
        s_state = NM_STATE_STA;
        ESP_LOGI(TAG, "STA conectado");
        return true;
    }

    s_state = NM_STATE_FALLBACK;
    ESP_LOGW(TAG, "STA fallo — volviendo a AP");
    esp_wifi_disconnect();
    return false;
}

// Llamado desde webserver cuando se guarda config MQTT
bool nm_connect_mqtt(Controller *ctrl) {
    if (s_state != NM_STATE_STA) {
        ESP_LOGW(TAG, "No hay STA activo");
        return false;
    }
    char broker[128] = {0};
    if (!config_get_mqtt(broker)) {
        ESP_LOGW(TAG, "Sin broker MQTT guardado");
        return false;
    }
    mqtt_manager_start(ctrl);
    vTaskDelay(pdMS_TO_TICKS(3000));
    if (mqtt_manager_is_connected()) {
        s_state = NM_STATE_MQTT;
        ESP_LOGI(TAG, "MQTT conectado");
        return true;
    }
    ESP_LOGW(TAG, "MQTT fallo");
    return false;
}

void nm_task(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

nm_state_t nm_get_state(void) {
    return s_state;
}

void nm_disconnect(void) {
    esp_wifi_disconnect();
    wifi_config_t empty = {0};
    esp_wifi_set_config(WIFI_IF_STA, &empty);
    s_state = NM_STATE_AP;
    ESP_LOGI("NM", "STA desconectado y config limpiada");
}
