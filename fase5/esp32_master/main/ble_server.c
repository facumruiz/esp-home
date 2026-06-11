#include "ble_server.h"
#include "ble_protocol.h"
#include "controller.h"
#include "config.h"
#include "config_manager.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "led.h"
#include "led_strip_ws.h"
#include "espnow_master.h"

// NimBLE headers (componente esp-nimble incluido en ESP-IDF >= 4.3)
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "BLE_SERVER";

// ─── Estado global ────────────────────────────────────────────────────────────

static Controller *s_ctrl = NULL;
static uint16_t    s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool        s_notify_ready = false;   // true cuando el cliente suscribe a notify

// Handles de características (necesarios para ble_gatts_notify)
static uint16_t s_handle_led1      = 0;
static uint16_t s_handle_strip1    = 0;
static uint16_t s_handle_strip2    = 0;
static uint16_t s_handle_lux       = 0;
static uint16_t s_handle_node      = 0;
static uint16_t s_handle_pir       = 0;
static uint16_t s_handle_wifi      = 0;
static uint16_t s_handle_mqtt      = 0;
static uint16_t s_handle_net_status = 0;

// ─── Helpers de serialización ─────────────────────────────────────────────────

// Serializa estado de una tira WS2812B a JSON compacto:
// {"s":1,"r":255,"g":100,"b":0,"br":128,"ef":0}
static int strip_to_json(LedStrip *s, char *buf, size_t buf_size) {
    return snprintf(buf, buf_size,
        "{\"s\":%d,\"r\":%d,\"g\":%d,\"b\":%d,\"br\":%d,\"ef\":%d}",
        s->state ? 1 : 0,
        s->r, s->g, s->b,
        s->brightness,
        (int)s->effect);
}

// Deserializa JSON de tira y aplica cambios al Controller
// Soporta: state (bool/"ON"/"OFF"), r/g/b, brightness, effect
static void apply_strip_json(LedStrip *strip, const char *data, int data_len) {
    char buf[256] = {0};
    int len = (data_len < (int)(sizeof(buf) - 1)) ? data_len : (int)(sizeof(buf) - 1);
    memcpy(buf, data, len);

    cJSON *root = cJSON_ParseWithLength(buf, len);
    if (!root) {
        ESP_LOGW(TAG, "JSON inválido para strip: %.*s", len, buf);
        return;
    }

    cJSON *state = cJSON_GetObjectItem(root, "s");
    if (cJSON_IsBool(state)) {
        led_strip_ws_set_state(strip, cJSON_IsTrue(state));
    } else if (cJSON_IsString(state)) {
        led_strip_ws_set_state(strip, strcmp(state->valuestring, "ON") == 0);
    } else if (cJSON_IsNumber(state)) {
        led_strip_ws_set_state(strip, state->valueint != 0);
    }

    cJSON *r = cJSON_GetObjectItem(root, "r");
    cJSON *g = cJSON_GetObjectItem(root, "g");
    cJSON *b = cJSON_GetObjectItem(root, "b");
    if (cJSON_IsNumber(r) && cJSON_IsNumber(g) && cJSON_IsNumber(b)) {
        led_strip_ws_set_color(strip,
            (uint8_t)r->valueint,
            (uint8_t)g->valueint,
            (uint8_t)b->valueint);
    }

    cJSON *br = cJSON_GetObjectItem(root, "br");
    if (cJSON_IsNumber(br)) {
        int val = br->valueint;
        if (val < 0)   val = 0;
        if (val > 255) val = 255;
        led_strip_ws_set_brightness(strip, (uint8_t)val);
    }

    cJSON *ef = cJSON_GetObjectItem(root, "ef");
    if (cJSON_IsNumber(ef)) {
        int effect = ef->valueint;
        if (effect < 0 || effect > 3) effect = 0;
        led_strip_ws_set_effect(strip, (strip_effect_t)effect);
    }

    if (strip->effect == STRIP_EFFECT_NONE) {
        led_strip_ws_apply(strip);
    }

    cJSON_Delete(root);
}

// ─── Función de notify genérica ───────────────────────────────────────────────

static void send_notify(uint16_t attr_handle, const uint8_t *data, uint16_t len) {
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !s_notify_ready) return;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (!om) {
        ESP_LOGE(TAG, "mbuf alloc failed para handle 0x%04X", attr_handle);
        return;
    }
    int rc = ble_gatts_notify_custom(s_conn_handle, attr_handle, om);
    if (rc != 0) {
        ESP_LOGW(TAG, "Notify falló (handle=0x%04X, rc=%d)", attr_handle, rc);
    }
}

// ─── Callbacks de lectura de características ─────────────────────────────────

static int cb_read_led1(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t val = (uint8_t)led_get_state(s_ctrl->led1);
    return os_mbuf_append(ctxt->om, &val, sizeof(val));
}

static int cb_read_strip1(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    char buf[128];
    strip_to_json(s_ctrl->strip1, buf, sizeof(buf));
    return os_mbuf_append(ctxt->om, buf, strlen(buf));
}

static int cb_read_strip2(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    char buf[128];
    strip_to_json(s_ctrl->strip2, buf, sizeof(buf));
    return os_mbuf_append(ctxt->om, buf, strlen(buf));
}

static int cb_read_lux(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg) {
    // Bit0=dark, Bit1=auto_mode
    uint8_t val = 0;
    if (g_node_status.dark) val |= 0x01;
    if (s_ctrl->auto_mode)  val |= 0x02;
    return os_mbuf_append(ctxt->om, &val, sizeof(val));
}

static int cb_read_node(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
    bool online = espnow_node_is_online();
    // 6 bytes: [enabled][online][dark][motion][pir_enabled][hold_ms lo][hold_ms hi]
    uint8_t buf[7];
    buf[0] = s_ctrl->node_enabled  ? 1 : 0;
    buf[1] = online                ? 1 : 0;
    buf[2] = g_node_status.dark    ? 1 : 0;
    buf[3] = g_node_status.motion  ? 1 : 0;
    buf[4] = s_ctrl->pir_enabled   ? 1 : 0;
    buf[5] = (uint8_t)(s_ctrl->pir_hold_remaining_ms & 0xFF);
    buf[6] = (uint8_t)(s_ctrl->pir_hold_remaining_ms >> 8);
    return os_mbuf_append(ctxt->om, buf, sizeof(buf));
}

static int cb_read_pir(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg) {
    // Bit0=pir_enabled, Bit1=motion, Bit2=keep_on
    uint8_t val = 0;
    if (s_ctrl->pir_enabled)  val |= 0x01;
    if (g_node_status.motion) val |= 0x02;
    if (s_ctrl->pir_keep_on)  val |= 0x04;
    return os_mbuf_append(ctxt->om, &val, sizeof(val));
}

static int cb_read_net_status(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    char ssid[64] = {0}, pass[64] = {0}, broker[128] = {0};
    bool wifi_cfg  = config_get_wifi(ssid, pass);
    bool mqtt_cfg  = config_get_mqtt(broker);
    nm_state_t st  = nm_get_state();
    bool wifi_conn = (st == NM_STATE_STA || st == NM_STATE_MQTT);
    bool mqtt_conn = (st == NM_STATE_MQTT);

    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"wc\":%d,\"ws\":\"%s\",\"mc\":%d,\"mb\":\"%s\"}",
        wifi_conn ? 1 : 0,
        wifi_cfg ? ssid : "",
        mqtt_conn ? 1 : 0,
        mqtt_cfg ? broker : "");
    return os_mbuf_append(ctxt->om, buf, strlen(buf));
}

// ─── Callbacks de escritura de características ────────────────────────────────

static int cb_write_led1(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (OS_MBUF_PKTLEN(ctxt->om) < 1) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    uint8_t cmd;
    os_mbuf_copydata(ctxt->om, 0, 1, &cmd);

    switch (cmd) {
        case BLE_CMD_LED_OFF:    led_off(s_ctrl->led1); break;
        case BLE_CMD_LED_ON:     led_on(s_ctrl->led1);  break;
        case BLE_CMD_LED_TOGGLE: led_toggle(s_ctrl->led1); break;
        default: return BLE_ATT_ERR_UNLIKELY;
    }

    ESP_LOGI(TAG, "LED1 cmd=0x%02X → state=%d", cmd, led_get_state(s_ctrl->led1));
    ble_server_notify_led1();
    return 0;
}

static int cb_write_strip1(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int len = OS_MBUF_PKTLEN(ctxt->om);
    if (len < 2) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    char buf[256] = {0};
    os_mbuf_copydata(ctxt->om, 0, (len < 255 ? len : 255), buf);
    apply_strip_json(s_ctrl->strip1, buf, len);
    ble_server_notify_strip1();
    return 0;
}

static int cb_write_strip2(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int len = OS_MBUF_PKTLEN(ctxt->om);
    if (len < 2) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    char buf[256] = {0};
    os_mbuf_copydata(ctxt->om, 0, (len < 255 ? len : 255), buf);
    apply_strip_json(s_ctrl->strip2, buf, len);
    ble_server_notify_strip2();
    return 0;
}

static int cb_write_lux(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t cmd;
    if (OS_MBUF_PKTLEN(ctxt->om) < 1) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    os_mbuf_copydata(ctxt->om, 0, 1, &cmd);

    if (cmd & BLE_CMD_AUTO_TOGGLE) {
        s_ctrl->auto_mode = !s_ctrl->auto_mode;
        ESP_LOGI(TAG, "Auto mode: %s", s_ctrl->auto_mode ? "ON" : "OFF");
    }
    ble_server_notify_lux();
    return 0;
}

static int cb_write_node(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t cmd;
    if (OS_MBUF_PKTLEN(ctxt->om) < 1) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    os_mbuf_copydata(ctxt->om, 0, 1, &cmd);

    if (cmd == BLE_CMD_NODE_TOGGLE) {
        s_ctrl->node_enabled = !s_ctrl->node_enabled;
        ESP_LOGI(TAG, "Node: %s", s_ctrl->node_enabled ? "enabled" : "disabled");
    }
    ble_server_notify_node();
    return 0;
}

static int cb_write_pir(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t cmd;
    if (OS_MBUF_PKTLEN(ctxt->om) < 1) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    os_mbuf_copydata(ctxt->om, 0, 1, &cmd);

    if (cmd == BLE_CMD_PIR_TOGGLE) {
        s_ctrl->pir_enabled = !s_ctrl->pir_enabled;
        ESP_LOGI(TAG, "PIR: %s", s_ctrl->pir_enabled ? "enabled" : "disabled");
    }
    ble_server_notify_pir();
    return 0;
}

static int cb_write_wifi(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int len = OS_MBUF_PKTLEN(ctxt->om);
    if (len < 1) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    uint8_t first_byte;
    os_mbuf_copydata(ctxt->om, 0, 1, &first_byte);

    // Comando especial: 0xFF = borrar credenciales
    if (first_byte == BLE_CMD_CLEAR && len == 1) {
        config_clear_wifi();
        nm_disconnect();
        ESP_LOGI(TAG, "WiFi config borrada");
        ble_server_notify_wifi_result(0x00);
        ble_server_notify_net_status();
        return 0;
    }

    // Formato: SSID\0PASSWORD (separados por null byte)
    char buf[256] = {0};
    os_mbuf_copydata(ctxt->om, 0, (len < 255 ? len : 255), buf);

    // Encontrar el null separator
    char *null_pos = memchr(buf, '\0', len);
    if (!null_pos) {
        ESP_LOGW(TAG, "WiFi write: falta separador \\0");
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    char *ssid = buf;
    char *pass = null_pos + 1;

    if (strlen(ssid) == 0) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    config_set_wifi(ssid, pass);

    // Conectar en un task separado para no bloquear el callback BLE
    // nm_connect_sta() tarda hasta 10s
    xTaskCreate([](void *arg) {
        bool ok = nm_connect_sta();
        ble_server_notify_wifi_result(ok ? 0x01 : 0x00);
        ble_server_notify_net_status();
        ESP_LOGI("BLE_WiFi", "Conexión STA: %s", ok ? "OK" : "FAIL");
        vTaskDelete(NULL);
    }, "ble_wifi_connect", 4096, NULL, 3, NULL);

    return 0;
}

static int cb_write_mqtt(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int len = OS_MBUF_PKTLEN(ctxt->om);
    if (len < 1) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;

    uint8_t first_byte;
    os_mbuf_copydata(ctxt->om, 0, 1, &first_byte);

    if (first_byte == BLE_CMD_CLEAR && len == 1) {
        config_clear_mqtt();
        mqtt_manager_stop();
        ESP_LOGI(TAG, "MQTT config borrada");
        ble_server_notify_mqtt_result(0x00);
        ble_server_notify_net_status();
        return 0;
    }

    char broker[128] = {0};
    os_mbuf_copydata(ctxt->om, 0, (len < 127 ? len : 127), broker);
    config_set_mqtt(broker);

    xTaskCreate([](void *arg) {
        bool ok = nm_connect_mqtt(s_ctrl);
        ble_server_notify_mqtt_result(ok ? 0x01 : 0x00);
        ble_server_notify_net_status();
        ESP_LOGI("BLE_MQTT", "Conexión MQTT: %s", ok ? "OK" : "FAIL");
        vTaskDelete(NULL);
    }, "ble_mqtt_connect", 4096, NULL, 3, NULL);

    return 0;
}

// ─── Callback unificado de acceso GATT ───────────────────────────────────────
// NimBLE usa una función por característica. Usamos un dispatcher.

typedef int (*gatt_access_fn)(uint16_t, uint16_t, struct ble_gatt_access_ctxt *, void *);

static int gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg) {
    gatt_access_fn fn_read  = ((gatt_access_fn *)arg)[0];
    gatt_access_fn fn_write = ((gatt_access_fn *)arg)[1];

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR && fn_read) {
        return fn_read(conn_handle, attr_handle, ctxt, NULL);
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR && fn_write) {
        return fn_write(conn_handle, attr_handle, ctxt, NULL);
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// ─── Definición de servicios GATT ─────────────────────────────────────────────
// NimBLE requiere arrays estáticos de ble_gatt_svc_def.

static gatt_access_fn s_led1_fns[]    = {cb_read_led1,    cb_write_led1};
static gatt_access_fn s_strip1_fns[]  = {cb_read_strip1,  cb_write_strip1};
static gatt_access_fn s_strip2_fns[]  = {cb_read_strip2,  cb_write_strip2};
static gatt_access_fn s_lux_fns[]     = {cb_read_lux,     cb_write_lux};
static gatt_access_fn s_node_fns[]    = {cb_read_node,    cb_write_node};
static gatt_access_fn s_pir_fns[]     = {cb_read_pir,     cb_write_pir};
static gatt_access_fn s_wifi_fns[]    = {NULL,            cb_write_wifi};
static gatt_access_fn s_mqtt_fns[]    = {NULL,            cb_write_mqtt};
static gatt_access_fn s_netst_fns[]   = {cb_read_net_status, NULL};

// Strings estáticos para strings de Device Info
static const char *s_manufacturer = BLE_MANUFACTURER_NAME;
static const char *s_model        = BLE_MODEL_NUMBER;
static const char *s_firmware     = BLE_FIRMWARE_REVISION;

static int cb_read_devinfo(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg) {
    const char *str = (const char *)arg;
    return os_mbuf_append(ctxt->om, str, strlen(str));
}

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    // ── Servicio 1: Device Control ─────────────────────────────────────────
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(
            0x00,0x00,0x01,0xAA, 0x00,0x00, 0x34,0x12,
            0x34,0x12, 0x34,0x12, 0x78,0x56,0x34,0x12),
        .characteristics = (struct ble_gatt_chr_def[]) {
            // LED1
            {
                .uuid = BLE_UUID128_DECLARE(
                    0x00,0x00,0x01,0xAA, 0x01,0x00, 0x34,0x12,
                    0x34,0x12, 0x34,0x12, 0x78,0x56,0x34,0x12),
                .access_cb  = gatt_access,
                .arg        = s_led1_fns,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_handle_led1,
            },
            // Strip1
            {
                .uuid = BLE_UUID128_DECLARE(
                    0x00,0x00,0x01,0xAA, 0x02,0x00, 0x34,0x12,
                    0x34,0x12, 0x34,0x12, 0x78,0x56,0x34,0x12),
                .access_cb  = gatt_access,
                .arg        = s_strip1_fns,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_handle_strip1,
            },
            // Strip2
            {
                .uuid = BLE_UUID128_DECLARE(
                    0x00,0x00,0x01,0xAA, 0x03,0x00, 0x34,0x12,
                    0x34,0x12, 0x34,0x12, 0x78,0x56,0x34,0x12),
                .access_cb  = gatt_access,
                .arg        = s_strip2_fns,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_handle_strip2,
            },
            { 0 } // terminator
        },
    },
    // ── Servicio 2: Sensor Status ───────────────────────────────────────────
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(
            0x00,0x00,0x02,0xAA, 0x00,0x00, 0x34,0x12,
            0x34,0x12, 0x34,0x12, 0x78,0x56,0x34,0x12),
        .characteristics = (struct ble_gatt_chr_def[]) {
            // Lux
            {
                .uuid = BLE_UUID128_DECLARE(
                    0x00,0x00,0x02,0xAA, 0x01,0x00, 0x34,0x12,
                    0x34,0x12, 0x34,0x12, 0x78,0x56,0x34,0x12),
                .access_cb  = gatt_access,
                .arg        = s_lux_fns,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_handle_lux,
            },
            // Node
            {
                .uuid = BLE_UUID128_DECLARE(
                    0x00,0x00,0x02,0xAA, 0x02,0x00, 0x34,0x12,
                    0x34,0x12, 0x34,0x12, 0x78,0x56,0x34,0x12),
                .access_cb  = gatt_access,
                .arg        = s_node_fns,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_handle_node,
            },
            // PIR
            {
                .uuid = BLE_UUID128_DECLARE(
                    0x00,0x00,0x02,0xAA, 0x03,0x00, 0x34,0x12,
                    0x34,0x12, 0x34,0x12, 0x78,0x56,0x34,0x12),
                .access_cb  = gatt_access,
                .arg        = s_pir_fns,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_handle_pir,
            },
            { 0 }
        },
    },
    // ── Servicio 3: Network Config ──────────────────────────────────────────
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(
            0x00,0x00,0x03,0xAA, 0x00,0x00, 0x34,0x12,
            0x34,0x12, 0x34,0x12, 0x78,0x56,0x34,0x12),
        .characteristics = (struct ble_gatt_chr_def[]) {
            // WiFi
            {
                .uuid = BLE_UUID128_DECLARE(
                    0x00,0x00,0x03,0xAA, 0x01,0x00, 0x34,0x12,
                    0x34,0x12, 0x34,0x12, 0x78,0x56,0x34,0x12),
                .access_cb  = gatt_access,
                .arg        = s_wifi_fns,
                .flags      = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_handle_wifi,
            },
            // MQTT
            {
                .uuid = BLE_UUID128_DECLARE(
                    0x00,0x00,0x03,0xAA, 0x02,0x00, 0x34,0x12,
                    0x34,0x12, 0x34,0x12, 0x78,0x56,0x34,0x12),
                .access_cb  = gatt_access,
                .arg        = s_mqtt_fns,
                .flags      = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_handle_mqtt,
            },
            // Net status
            {
                .uuid = BLE_UUID128_DECLARE(
                    0x00,0x00,0x03,0xAA, 0x03,0x00, 0x34,0x12,
                    0x34,0x12, 0x34,0x12, 0x78,0x56,0x34,0x12),
                .access_cb  = gatt_access,
                .arg        = s_netst_fns,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &s_handle_net_status,
            },
            { 0 }
        },
    },
    // ── Servicio 4: Device Info (BT SIG estándar) ───────────────────────────
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x180A),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(0x2A29), // Manufacturer
                .access_cb = cb_read_devinfo,
                .arg       = (void *)s_manufacturer,
                .flags     = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2A24), // Model
                .access_cb = cb_read_devinfo,
                .arg       = (void *)s_model,
                .flags     = BLE_GATT_CHR_F_READ,
            },
            {
                .uuid = BLE_UUID16_DECLARE(0x2A26), // Firmware revision
                .access_cb = cb_read_devinfo,
                .arg       = (void *)s_firmware,
                .flags     = BLE_GATT_CHR_F_READ,
            },
            { 0 }
        },
    },
    { 0 } // terminator
};

// ─── Callbacks de eventos GAP (conexión/desconexión) ─────────────────────────

static int on_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            s_notify_ready = false; // el cliente debe suscribirse a notify explícitamente
            ESP_LOGI(TAG, "Cliente BLE conectado — handle=%d", s_conn_handle);

            // Negociar MTU máximo para reducir fragmentación
            ble_att_set_preferred_mtu(512);
            ble_gattc_exchange_mtu(s_conn_handle, NULL, NULL);
        } else {
            ESP_LOGW(TAG, "Conexión fallida, status=%d — reiniciando advertising",
                     event->connect.status);
            ble_server_start();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Cliente BLE desconectado — razón=%d", event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_notify_ready = false;
        // Reiniciar advertising automáticamente
        ble_server_start();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        // El cliente ha activado/desactivado notify para una característica
        if (event->subscribe.cur_notify) {
            s_notify_ready = true;
            ESP_LOGI(TAG, "Cliente suscrito a notify — attr=0x%04X",
                     event->subscribe.attr_handle);
            // Enviar estado completo inmediatamente al conectarse
            ble_server_notify_led1();
            ble_server_notify_strip1();
            ble_server_notify_strip2();
            ble_server_notify_lux();
            ble_server_notify_node();
            ble_server_notify_pir();
            ble_server_notify_net_status();
        }
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU negociado: %d bytes", event->mtu.value);
        break;

    default:
        break;
    }
    return 0;
}

// ─── Sincronización del stack BLE ────────────────────────────────────────────

static void on_ble_sync(void) {
    // El stack BLE está listo — iniciar advertising
    ESP_LOGI(TAG, "BLE stack sincronizado");
    ble_server_start();
}

static void on_ble_reset(int reason) {
    ESP_LOGW(TAG, "BLE stack reseteado, razón=%d", reason);
}

// ─── NimBLE host task ─────────────────────────────────────────────────────────

static void nimble_host_task(void *param) {
    ESP_LOGI(TAG, "NimBLE host task iniciada");
    nimble_port_run();       // Bloquea hasta nimble_port_stop()
    nimble_port_freertos_deinit();
}

// ─── API pública ──────────────────────────────────────────────────────────────

void ble_server_init(Controller *ctrl) {
    s_ctrl = ctrl;

    nimble_port_init();

    // Configurar callbacks del host
    ble_hs_cfg.sync_cb  = on_ble_sync;
    ble_hs_cfg.reset_cb = on_ble_reset;

    // Registrar servicios GATT
    int rc = ble_gatts_count_cfg(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg error: %d", rc);
        return;
    }
    rc = ble_gatts_add_svcs(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs error: %d", rc);
        return;
    }

    // Configurar nombre del dispositivo GAP
    ble_svc_gap_device_name_set(BLE_DEVICE_NAME);
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Iniciar task del host NimBLE
    nimble_port_freertos_init(nimble_host_task);

    ESP_LOGI(TAG, "BLE server inicializado — dispositivo: %s", BLE_DEVICE_NAME);
}

void ble_server_start(void) {
    // Configurar advertising data
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name         = (uint8_t *)BLE_DEVICE_NAME;
    fields.name_len     = strlen(BLE_DEVICE_NAME);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields error: %d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode  = BLE_GAP_CONN_MODE_UND;  // Undirected connectable
    adv_params.disc_mode  = BLE_GAP_DISC_MODE_GEN;  // General discoverable
    adv_params.itvl_min   = BLE_GAP_ADV_ITVL_MS(BLE_ADV_INTERVAL_MS);
    adv_params.itvl_max   = BLE_GAP_ADV_ITVL_MS(BLE_ADV_INTERVAL_MS * 2);

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, on_gap_event, NULL);
    if (rc == BLE_HS_EALREADY) {
        ESP_LOGD(TAG, "Advertising ya activo");
        return;
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start error: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "BLE advertising activo — esperando conexiones");
}

void ble_server_stop(void) {
    ble_gap_adv_stop();
    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    ESP_LOGI(TAG, "BLE server detenido");
}

bool ble_server_is_connected(void) {
    return s_conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

// ─── Funciones de notify ──────────────────────────────────────────────────────

void ble_server_notify_led1(void) {
    uint8_t val = (uint8_t)led_get_state(s_ctrl->led1);
    send_notify(s_handle_led1, &val, 1);
}

void ble_server_notify_strip1(void) {
    char buf[128];
    int len = strip_to_json(s_ctrl->strip1, buf, sizeof(buf));
    send_notify(s_handle_strip1, (uint8_t *)buf, (uint16_t)len);
}

void ble_server_notify_strip2(void) {
    char buf[128];
    int len = strip_to_json(s_ctrl->strip2, buf, sizeof(buf));
    send_notify(s_handle_strip2, (uint8_t *)buf, (uint16_t)len);
}

void ble_server_notify_lux(void) {
    uint8_t val = 0;
    if (g_node_status.dark) val |= 0x01;
    if (s_ctrl->auto_mode)  val |= 0x02;
    send_notify(s_handle_lux, &val, 1);
}

void ble_server_notify_node(void) {
    bool online = espnow_node_is_online();
    uint8_t buf[7];
    buf[0] = s_ctrl->node_enabled  ? 1 : 0;
    buf[1] = online                ? 1 : 0;
    buf[2] = g_node_status.dark    ? 1 : 0;
    buf[3] = g_node_status.motion  ? 1 : 0;
    buf[4] = s_ctrl->pir_enabled   ? 1 : 0;
    buf[5] = (uint8_t)(s_ctrl->pir_hold_remaining_ms & 0xFF);
    buf[6] = (uint8_t)(s_ctrl->pir_hold_remaining_ms >> 8);
    send_notify(s_handle_node, buf, sizeof(buf));
}

void ble_server_notify_pir(void) {
    uint8_t val = 0;
    if (s_ctrl->pir_enabled)  val |= 0x01;
    if (g_node_status.motion) val |= 0x02;
    if (s_ctrl->pir_keep_on)  val |= 0x04;
    send_notify(s_handle_pir, &val, 1);
}

void ble_server_notify_wifi_result(uint8_t result) {
    send_notify(s_handle_wifi, &result, 1);
}

void ble_server_notify_mqtt_result(uint8_t result) {
    send_notify(s_handle_mqtt, &result, 1);
}

void ble_server_notify_net_status(void) {
    char ssid[64] = {0}, pass[64] = {0}, broker[128] = {0};
    bool wifi_cfg  = config_get_wifi(ssid, pass);
    bool mqtt_cfg  = config_get_mqtt(broker);
    nm_state_t st  = nm_get_state();
    bool wifi_conn = (st == NM_STATE_STA || st == NM_STATE_MQTT);
    bool mqtt_conn = (st == NM_STATE_MQTT);

    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "{\"wc\":%d,\"ws\":\"%s\",\"mc\":%d,\"mb\":\"%s\"}",
        wifi_conn ? 1 : 0,
        wifi_cfg ? ssid : "",
        mqtt_conn ? 1 : 0,
        mqtt_cfg ? broker : "");
    send_notify(s_handle_net_status, (uint8_t *)buf, (uint16_t)len);
}
