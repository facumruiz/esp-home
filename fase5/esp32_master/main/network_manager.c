/*
 * network_manager.c — versión BLE
 *
 * CAMBIOS respecto a la versión WiFi:
 *   - ELIMINADO: modo AP (esp_netif_create_default_wifi_ap, ap_cfg)
 *   - ELIMINADO: estado NM_STATE_FALLBACK (ya no hay AP de fallback)
 *   - MANTENIDO: modo STA para WiFi doméstico
 *   - MANTENIDO: integración MQTT para Home Assistant
 *   - AÑADIDO: auto-reconexión cuando se pierda la conexión STA
 *   - AÑADIDO: intento de conexión automática al arrancar si hay NVS creds
 *
 * La configuración de red ahora llega exclusivamente por BLE
 * (característica BLE_UUID_CHAR_WIFI y BLE_UUID_CHAR_MQTT).
 */

#include "network_manager.h"
#include "config_manager.h"
#include "mqtt_manager.h"
#include "config.h"
#include "ble_server.h"
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
static bool s_auto_reconnect = true;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "STA desconectado");
        xEventGroupSetBits(s_wifi_eg, WIFI_FAIL_BIT);
        if (s_auto_reconnect && s_state == NM_STATE_STA) {
            ESP_LOGI(TAG, "Intentando reconexión automática...");
            esp_wifi_connect();
        }
    }
    if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP obtenida: " IPSTR, IP2STR(&ev->ip_info.ip));
        xEventGroupSetBits(s_wifi_eg, WIFI_CONNECTED_BIT);
    }
}

static void nm_auto_connect_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    nm_connect_sta();
    vTaskDelete(NULL);
}

void nm_init(Controller *ctrl) {
    (void)ctrl;
    s_wifi_eg = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        wifi_event_handler, NULL, NULL);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi STA iniciado (sin AP)");

    char ssid[64] = {0}, pass[64] = {0};
    if (config_get_wifi(ssid, pass)) {
        ESP_LOGI(TAG, "Credenciales NVS encontradas (%s) — conexión automática", ssid);
        xTaskCreate(nm_auto_connect_task, "nm_auto_conn", 4096, NULL, 3, NULL);
    }
}

bool nm_connect_sta(void) {
    char ssid[64] = {0};
    char pass[64] = {0};
    if (!config_get_wifi(ssid, pass)) {
        ESP_LOGW(TAG, "Sin credenciales WiFi guardadas");
        return false;
    }

    s_auto_reconnect = false;
    s_state = NM_STATE_CONNECTING;

    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid,     ssid, 31);
    strncpy((char *)sta_cfg.sta.password, pass, 63);
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    esp_wifi_connect();

    xEventGroupClearBits(s_wifi_eg, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    EventBits_t bits = xEventGroupWaitBits(s_wifi_eg,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & WIFI_CONNECTED_BIT) {
        s_state = NM_STATE_STA;
        s_auto_reconnect = true;
        ESP_LOGI(TAG, "WiFi STA conectado a %s", ssid);

        char broker[128] = {0};
        if (config_get_mqtt(broker)) {
            nm_connect_mqtt(NULL);
        }

        ble_server_notify_net_status();
        return true;
    }

    s_state = NM_STATE_AP;
    ESP_LOGW(TAG, "WiFi STA falló para %s", ssid);
    ble_server_notify_net_status();
    return false;
}

bool nm_connect_mqtt(Controller *ctrl) {
    if (s_state != NM_STATE_STA && s_state != NM_STATE_MQTT) {
        ESP_LOGW(TAG, "No hay WiFi STA activo para MQTT");
        return false;
    }
    char broker[128] = {0};
    if (!config_get_mqtt(broker)) {
        ESP_LOGW(TAG, "Sin broker MQTT guardado");
        return false;
    }
    if (ctrl) mqtt_manager_start(ctrl);
    vTaskDelay(pdMS_TO_TICKS(3000));
    if (mqtt_manager_is_connected()) {
        s_state = NM_STATE_MQTT;
        ESP_LOGI(TAG, "MQTT conectado → %s", broker);
        ble_server_notify_net_status();
        return true;
    }
    ESP_LOGW(TAG, "MQTT falló para %s", broker);
    return false;
}

void nm_task(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        if (s_state == NM_STATE_STA && !mqtt_manager_is_connected()) {
            char broker[128] = {0};
            if (config_get_mqtt(broker)) {
                ESP_LOGI(TAG, "Reintentando MQTT...");
                nm_connect_mqtt(NULL);
            }
        }
    }
}

nm_state_t nm_get_state(void) {
    return s_state;
}

void nm_disconnect(void) {
    s_auto_reconnect = false;
    esp_wifi_disconnect();
    wifi_config_t empty = {0};
    esp_wifi_set_config(WIFI_IF_STA, &empty);
    s_state = NM_STATE_AP;
    ESP_LOGI(TAG, "WiFi STA desconectado");
    ble_server_notify_net_status();
}
