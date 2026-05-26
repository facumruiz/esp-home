file(GLOB U8G2_SOURCES "u8g2/*.c")

idf_component_register(
    SRCS
        "main.c"
        "led.c"
        "led_strip_ws.c"
        "lux_sensor.c"
        "controller.c"
        "network_manager.c"
        "mqtt_manager.c"
        "espnow_master.c"
        "webserver.c"
        "oled_display.c"
        "qrcodegen.c"
        "u8g2_esp32_hal.c"
        ${U8G2_SOURCES}
    INCLUDE_DIRS "." "u8g2"
    REQUIRES
        esp_wifi esp_event esp_netif nvs_flash
        esp_http_server json led_strip mqtt
        driver esp_timer
)
