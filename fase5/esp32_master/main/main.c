/*
 * main.c — ESP-HOME BLE
 *
 * Migración de WiFi AP + HTTP WebServer a BLE GATT Server.
 *
 * CAMBIOS respecto a la versión WiFi:
 *   - ELIMINADO: task webserver_task (HTTP server)
 *   - ELIMINADO: WiFi AP (modo solo AP no es necesario)
 *   - AÑADIDO:   ble_server_init() / ble_server_start()
 *   - MANTENIDO: WiFi STA + MQTT (opcional, configurable por BLE)
 *   - MANTENIDO: ESP-NOW para nodo sensor
 *   - MANTENIDO: todas las tasks de hardware (strips, lux, oled, net_led)
 *
 * Los notify BLE se disparan desde las tasks existentes mediante
 * ble_server_notify_*() — no hay polling, es event-driven puro.
 */

#include "config.h"
#include "led.h"
#include "led_strip_ws.h"
#include "lux_sensor.h"
#include "controller.h"
#include "espnow_master.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "ble_server.h"          // ← NUEVO: reemplaza webserver.h
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "oled_display.h"

static const char *TAG = "MAIN";

// ─── Task: sensor de luz + auto_mode + BLE notify ────────────────────────────
// Sin cambios en lógica de negocio. Se añade notify BLE al final.

void lux_task(void *arg) {
    Controller *ctrl = (Controller *)arg;
    while (1) {
        bool node_dark = ctrl->node_enabled && g_node_status.dark;
        if (ctrl->auto_mode) {
            led_strip_ws_set_state(ctrl->strip1, node_dark);
        }
        // MQTT sigue funcionando si está conectado
        mqtt_manager_publish_lux(ctrl);

        // Notificar cambio de estado por BLE (debounced internamente)
        ble_server_notify_lux();
        ble_server_notify_strip1();

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// ─── Task: LED de red (indica estado WiFi/MQTT) ────────────────────────────
// Sin cambios — el LED1 de GPIO2 sigue siendo el indicador de red.
// Nota: en la versión BLE, el LED ya no refleja el estado del AP (que no existe),
// sino el estado de la conexión WiFi STA / MQTT opcional.

void net_led_task(void *arg) {
    Controller *ctrl = (Controller *)arg;
    while (1) {
        nm_state_t st = nm_get_state();
        if (st == NM_STATE_MQTT) {
            // MQTT conectado → LED fijo
            led_on(ctrl->led1);
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else if (st == NM_STATE_STA) {
            // WiFi STA pero sin MQTT → parpadeo rápido
            led_on(ctrl->led1);
            vTaskDelay(pdMS_TO_TICKS(300));
            led_off(ctrl->led1);
            vTaskDelay(pdMS_TO_TICKS(300));
        } else {
            // Sin red → LED apagado
            led_off(ctrl->led1);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        // Notificar estado del LED (útil para la UI BLE)
        ble_server_notify_led1();
    }
}

// ─── app_main ─────────────────────────────────────────────────────────────────

void app_main(void) {
    // NVS: requerido por NimBLE y config_manager
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // ── Inicializar hardware ───────────────────────────────────────────────
    Led led1;
    led_init(&led1, LED1_PIN);

    LedStrip strip1;
    led_strip_ws_init(&strip1, STRIP_GPIO, STRIP_NUM_LEDS);

    LedStrip strip2;
    led_strip_ws_init(&strip2, STRIP2_GPIO, STRIP2_NUM_LEDS);

    LuxSensor lux1;
    lux_sensor_init(&lux1, LUX_GPIO);

    // ── Inicializar controlador ────────────────────────────────────────────
    Controller controller;
    controller_init(&controller, &led1, &strip1, &strip2, &lux1);

    // ── Inicializar red (solo STA, sin AP) ────────────────────────────────
    // nm_init() ahora no levanta el AP — ver network_manager.c modificado.
    // Si hay credenciales guardadas en NVS, intenta conectar automáticamente.
    nm_init(&controller);

    // ── Inicializar ESP-NOW para el nodo sensor ────────────────────────────
    // Sin cambios: el nodo sigue enviando dark+motion por ESP-NOW.
    espnow_master_init_with_ctrl(&controller);

    // ── Inicializar servidor BLE (reemplaza webserver) ────────────────────
    ble_server_init(&controller);
    // ble_server_start() es llamado internamente por on_ble_sync()
    // cuando el stack NimBLE está listo.

    ESP_LOGI(TAG, "Sistema ESP-HOME BLE iniciado");

    // ── Lanzar tasks ──────────────────────────────────────────────────────
    // Mismas prioridades y stack sizes que la versión WiFi.
    // Se elimina: webserver_task (ya no existe)
    // Se mantienen: strip tasks, oled, net_led, lux

    xTaskCreate(led_strip_ws_task, "strip1_task", 4096, &strip1,     3, NULL);
    xTaskCreate(led_strip_ws_task, "strip2_task", 4096, &strip2,     3, NULL);
    xTaskCreate(oled_task,         "oled_task",   8192, &controller, 3, NULL);
    xTaskCreate(nm_task,           "net_manager", 4096, &controller, 4, NULL);
    xTaskCreate(net_led_task,      "net_led",     2048, &controller, 2, NULL);
    xTaskCreate(lux_task,          "lux_task",    4096, &controller, 2, NULL);

    // Loop principal: watchdog + logging de estado BLE periódico
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "BLE: %s | WiFi: %s",
                 ble_server_is_connected() ? "conectado" : "advertising",
                 nm_get_state() == NM_STATE_MQTT ? "MQTT" :
                 nm_get_state() == NM_STATE_STA  ? "STA"  : "offline");
    }
}
