idf_component_register(
    SRCS
        "main.c"
        "led.c"
        "controller.c"
        "webserver.c"
        "network_manager.c"
        "mqtt_manager.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
        mqtt
        driver
)
