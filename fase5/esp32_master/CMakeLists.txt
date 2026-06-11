file(GLOB U8G2_SOURCES "u8g2/*.c")

idf_component_register(
    SRCS
        "main.c"
        "led.c"
        "led_strip_ws.c"
        "lux_sensor.c"
        "controller.c"
        "network_manager.c"
        "config_manager.c"
        "mqtt_manager.c"
        "espnow_master.c"
        "ble_server.c"           # NUEVO: reemplaza webserver.c
        "oled_display.c"
        "qrcodegen.c"
        "u8g2_esp32_hal.c"
        ${U8G2_SOURCES}
    INCLUDE_DIRS "." "u8g2"
    REQUIRES
        esp_wifi esp_event esp_netif nvs_flash
        json led_strip mqtt
        driver esp_timer
        # BLE: NimBLE (incluido en ESP-IDF >= 4.4)
        bt
        # esp_http_server ya NO es necesario
)

# Habilitar NimBLE en sdkconfig si no está habilitado
# En sdkconfig.defaults agregar:
#   CONFIG_BT_ENABLED=y
#   CONFIG_BT_NIMBLE_ENABLED=y
#   CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1
#   CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=512
#   CONFIG_BT_CONTROLLER_MODE_BLE_ONLY=y
#   CONFIG_BT_BLUEDROID_ENABLED=n
