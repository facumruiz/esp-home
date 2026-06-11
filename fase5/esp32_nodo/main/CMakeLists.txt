idf_component_register(
    SRCS
        "main.c"
        "espnow_node.c"
        "lux_sensor.c"
        "pir_sensor.c"
        "led.c"
        "led_strip_ws.c"
        "controller.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        led_strip
)
