#include "config_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

#define NVS_NAMESPACE "esp_home"
static const char *TAG = "CFG";

void config_manager_init(void) {
    ESP_LOGI(TAG, "Config manager listo");
}

void config_set_wifi(const char *ssid, const char *password) {
    nvs_handle_t h;
    nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    nvs_set_str(h, "wifi_ssid", ssid);
    nvs_set_str(h, "wifi_pass", password);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "WiFi guardado: %s", ssid);
}

bool config_get_wifi(char *ssid, char *password) {
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    size_t len = 64;
    bool ok = true;
    if (nvs_get_str(h, "wifi_ssid", ssid, &len) != ESP_OK) ok = false;
    len = 64;
    if (nvs_get_str(h, "wifi_pass", password, &len) != ESP_OK) ok = false;
    nvs_close(h);
    return ok;
}

void config_set_mqtt(const char *broker_url) {
    nvs_handle_t h;
    nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    nvs_set_str(h, "mqtt_url", broker_url);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "MQTT guardado: %s", broker_url);
}

bool config_get_mqtt(char *broker_url) {
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    size_t len = 128;
    bool ok = nvs_get_str(h, "mqtt_url", broker_url, &len) == ESP_OK;
    nvs_close(h);
    return ok;
}

void config_clear_wifi(void) {
    nvs_handle_t h;
    nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    nvs_erase_key(h, "wifi_ssid");
    nvs_erase_key(h, "wifi_pass");
    nvs_commit(h);
    nvs_close(h);
}

void config_clear_mqtt(void) {
    nvs_handle_t h;
    nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    nvs_erase_key(h, "mqtt_url");
    nvs_commit(h);
    nvs_close(h);
}
