idf_component_register(
    SRCS
        "main.c"
        "espnow_node.c"
        "lux_sensor.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        led_strip
        json
)
