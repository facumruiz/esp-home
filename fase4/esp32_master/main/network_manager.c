#include "espnow_master.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "config.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdatomic.h>

static const char *TAG = "NM";

// Estado compartido (atómico para lectura segura desde otras tareas)
static atomic_int s_state = NM_STATE_AP;

// Event group para señales de WiFi
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_eg;
static esp_netif_t *s_netif_ap  = NULL;
static esp_netif_t *s_netif_sta = NULL;

// ─── Handlers de eventos WiFi ────────────────────────────────

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
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

// ─── Init AP (siempre activo como base) ──────────────────────

static void wifi_start_ap(void)
{
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
    ESP_LOGI(TAG, "AP activo: %s", WIFI_AP_SSID);
}

// ─── Intento de conexión STA ─────────────────────────────────

static bool try_connect_sta(void)
{
    ESP_LOGI(TAG, "Intentando conectar a STA: %s", WIFI_STA_SSID);

    wifi_config_t sta_cfg = {
        .sta = {
            .ssid     = WIFI_STA_SSID,
            .password = WIFI_STA_PASS,
        },
    };
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_eg,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdTRUE,   // limpiar bits al salir
        pdFALSE,  // cualquiera de los dos
        pdMS_TO_TICKS(NM_CONNECT_TIMEOUT_MS)
    );

    if (bits & WIFI_CONNECTED_BIT) {
        return true;
    }

    ESP_LOGW(TAG, "STA falló o timeout");
    esp_wifi_disconnect();
    return false;
}

// ─── Init general (una sola vez) ─────────────────────────────

void nm_init(Controller *ctrl)
{
    (void)ctrl;

    s_wifi_eg = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_netif_ap  = esp_netif_create_default_wifi_ap();
    s_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Registrar handlers
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        wifi_event_handler, NULL, NULL);

    // Modo APSTA: AP siempre activo, STA para intentos
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    wifi_start_ap();
    ESP_ERROR_CHECK(esp_wifi_start());
    espnow_master_init();
}

// ─── Tarea principal ─────────────────────────────────────────

void nm_task(void *pvParameters)
{
    Controller *ctrl = (Controller *)pvParameters;
    atomic_store(&s_state, NM_STATE_AP);
    vTaskDelay(pdMS_TO_TICKS(2000)); // esperar AP listo

    ESP_LOGI(TAG, "Iniciando en MODO_AP");

    while (1) {
        nm_state_t current = atomic_load(&s_state);

        switch (current) {

        case NM_STATE_AP:
            // Intentar pasar a STA una vez al inicio
            atomic_store(&s_state, NM_STATE_CONNECTING);
            break;

        case NM_STATE_CONNECTING:
            if (try_connect_sta()) {
                ESP_LOGI(TAG, "→ MODO_STA");
                mqtt_manager_start(ctrl);
                atomic_store(&s_state, NM_STATE_STA);
            } else {
                ESP_LOGW(TAG, "→ FALLBACK (AP sigue activo)");
                atomic_store(&s_state, NM_STATE_FALLBACK);
            }
            break;

        case NM_STATE_STA:
            // Monitorear: si MQTT o WiFi caen → FALLBACK
            if (!mqtt_manager_is_connected()) {
                ESP_LOGW(TAG, "MQTT caído → FALLBACK");
                mqtt_manager_stop();
                atomic_store(&s_state, NM_STATE_FALLBACK);
            }
            vTaskDelay(pdMS_TO_TICKS(3000));
            break;

        case NM_STATE_FALLBACK:
            // AP siempre activo, reintentar STA periódicamente
            ESP_LOGI(TAG, "FALLBACK: reintentando en %d ms", NM_RETRY_INTERVAL_MS);
            vTaskDelay(pdMS_TO_TICKS(NM_RETRY_INTERVAL_MS));
            atomic_store(&s_state, NM_STATE_CONNECTING);
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ─── Getter de estado ────────────────────────────────────────

nm_state_t nm_get_state(void)
{
    return (nm_state_t)atomic_load(&s_state);
}
