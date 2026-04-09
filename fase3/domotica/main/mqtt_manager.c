#include "mqtt_manager.h"
#include "config.h"
#include "led.h"

#include "mqtt_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdatomic.h>

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t s_client   = NULL;
static atomic_int               s_connected = 0;
static Controller              *s_ctrl      = NULL;

// ─── Handler de eventos MQTT ─────────────────────────────────

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Conectado al broker");
        atomic_store(&s_connected, 1);

        // 🔹 Disponibilidad ONLINE (retain)
        esp_mqtt_client_publish(s_client, MQTT_TOPIC_AVAIL,
                                "online", 0, 1, 1);

        // Suscribirse a comandos
        esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_LED_CMD, 1);

        // Publicar estado actual al conectar
        if (s_ctrl) mqtt_manager_publish_status(s_ctrl);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Desconectado del broker");
        atomic_store(&s_connected, 0);

        // 🔹 Disponibilidad OFFLINE (retain)
        esp_mqtt_client_publish(s_client, MQTT_TOPIC_AVAIL,
                                "offline", 0, 1, 1);
        break;

    case MQTT_EVENT_DATA:
        // Procesar comandos entrantes
        if (event->topic_len > 0 &&
            strncmp(event->topic, MQTT_TOPIC_LED_CMD, event->topic_len) == 0)
        {
            if (event->data_len >= 1) {
                if (event->data[0] == '1') {
                    ESP_LOGI(TAG, "CMD: LED ON");
                    led_on(s_ctrl->led1);
                } else if (event->data[0] == '0') {
                    ESP_LOGI(TAG, "CMD: LED OFF");
                    led_off(s_ctrl->led1);
                }

                // Confirmar estado después del comando
                mqtt_manager_publish_status(s_ctrl);
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Error MQTT");
        break;

    default:
        break;
    }
}

// ─── API pública ─────────────────────────────────────────────

void mqtt_manager_start(Controller *ctrl)
{
    s_ctrl = ctrl;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.client_id = MQTT_CLIENT_ID,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);

    esp_mqtt_client_start(s_client);

    ESP_LOGI(TAG, "Cliente MQTT iniciado → %s", MQTT_BROKER_URI);
}

void mqtt_manager_stop(void)
{
    if (s_client) {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        atomic_store(&s_connected, 0);
        ESP_LOGI(TAG, "Cliente MQTT detenido");
    }
}

bool mqtt_manager_is_connected(void)
{
    return atomic_load(&s_connected) == 1;
}

void mqtt_manager_publish_status(Controller *ctrl)
{
    if (!s_client || !atomic_load(&s_connected)) return;

    char payload[2] = { '0' + controller_get_status(ctrl), '\0' };

    // 🔹 Estado con retain (clave para Home Assistant)
    esp_mqtt_client_publish(s_client, MQTT_TOPIC_LED_STATUS,
                            payload, 1, 1, 1);

    ESP_LOGI(TAG, "Status publicado: %s", payload);
}
