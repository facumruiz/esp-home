#include "mqtt_manager.h"
#include "config.h"
#include "led.h"
#include "led_strip_ws.h"
#include "cJSON.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdatomic.h>

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t s_client    = NULL;
static atomic_int               s_connected = 0;
static Controller              *s_ctrl      = NULL;

// ─── Handle comando LED ───────────────────────────────────────────────────────

static void handle_led_command(Controller *ctrl, const char *payload, int payload_len) {
    char buf[256] = {0};
    int  len = payload_len < (int)(sizeof(buf) - 1) ? payload_len : (int)(sizeof(buf) - 1);
    memcpy(buf, payload, len);

    // Compatibilidad "0" / "1"
    if (len == 1 && buf[0] == '0') {
        led_off(ctrl->led1);
        led_strip_ws_set_state(ctrl->strip1, false);
        led_strip_ws_apply(ctrl->strip1);
        return;
    }
    if (len == 1 && buf[0] == '1') {
        led_on(ctrl->led1);
        led_strip_ws_set_state(ctrl->strip1, true);
        led_strip_ws_apply(ctrl->strip1);
        return;
    }

    // JSON
    cJSON *root = cJSON_ParseWithLength(buf, len);
    if (!root) {
        ESP_LOGW(TAG, "Payload inválido: %s", buf);
        return;
    }

    cJSON *state = cJSON_GetObjectItem(root, "state");
    if (cJSON_IsString(state)) {
        bool on = (strcmp(state->valuestring, "ON") == 0);
        led_strip_ws_set_state(ctrl->strip1, on);
        if (on) led_on(ctrl->led1); else led_off(ctrl->led1);
    }

    cJSON *brightness = cJSON_GetObjectItem(root, "brightness");
    if (cJSON_IsNumber(brightness)) {
        int val = brightness->valueint;
        if (val < 0)   val = 0;
        if (val > 255) val = 255;
        led_strip_ws_set_brightness(ctrl->strip1, (uint8_t)val);
    }

    cJSON *color = cJSON_GetObjectItem(root, "color");
    if (cJSON_IsObject(color)) {
        cJSON *r = cJSON_GetObjectItem(color, "r");
        cJSON *g = cJSON_GetObjectItem(color, "g");
        cJSON *b = cJSON_GetObjectItem(color, "b");
        if (cJSON_IsNumber(r) && cJSON_IsNumber(g) && cJSON_IsNumber(b)) {
            led_strip_ws_set_color(ctrl->strip1,
                (uint8_t)r->valueint,
                (uint8_t)g->valueint,
                (uint8_t)b->valueint);
        }
    }

    cJSON *effect = cJSON_GetObjectItem(root, "effect");
    if (cJSON_IsString(effect)) {
        strip_effect_t ef = STRIP_EFFECT_NONE;
        if      (strcmp(effect->valuestring, "blink")   == 0) ef = STRIP_EFFECT_BLINK;
        else if (strcmp(effect->valuestring, "fade")    == 0) ef = STRIP_EFFECT_FADE;
        else if (strcmp(effect->valuestring, "rainbow") == 0) ef = STRIP_EFFECT_RAINBOW;
        led_strip_ws_set_effect(ctrl->strip1, ef);
    }

    if (ctrl->strip1->effect == STRIP_EFFECT_NONE) {
        led_strip_ws_apply(ctrl->strip1);
    }

    cJSON_Delete(root);
}

// ─── Handler de eventos MQTT ──────────────────────────────────────────────────

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Conectado al broker");
        atomic_store(&s_connected, 1);
        esp_mqtt_client_publish(s_client, MQTT_TOPIC_AVAIL, "online", 0, 1, 1);
        esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_LED_CMD, 1);
        if (s_ctrl) mqtt_manager_publish_status(s_ctrl);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Desconectado del broker");
        atomic_store(&s_connected, 0);
        esp_mqtt_client_publish(s_client, MQTT_TOPIC_AVAIL, "offline", 0, 1, 1);
        break;

    case MQTT_EVENT_DATA:
        if (event->topic_len > 0 &&
            strncmp(event->topic, MQTT_TOPIC_LED_CMD, event->topic_len) == 0)
        {
            if (event->data_len >= 1) {
                ESP_LOGI(TAG, "CMD recibido");
                handle_led_command(s_ctrl, event->data, event->data_len);
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

// ─── API pública ──────────────────────────────────────────────────────────────

void mqtt_manager_start(Controller *ctrl)
{
    s_ctrl = ctrl;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri     = MQTT_BROKER_URI,
        .credentials.client_id  = MQTT_CLIENT_ID,
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
    esp_mqtt_client_publish(s_client, MQTT_TOPIC_LED_STATUS, payload, 1, 1, 1);
    ESP_LOGI(TAG, "Status publicado: %s", payload);
}
