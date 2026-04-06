#include "webserver.h"
#include "esp_http_server.h"
#include <stdio.h>

static Controller *global_ctrl;

// ----------- HANDLERS API -----------

esp_err_t led_on_handler(httpd_req_t *req) {
    led_on(global_ctrl->led1);
    httpd_resp_send(req, "LED1 ENCENDIDO", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t led_off_handler(httpd_req_t *req) {
    led_off(global_ctrl->led1);
    httpd_resp_send(req, "LED1 APAGADO", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t toggle_handler(httpd_req_t *req) {
    led_toggle(global_ctrl->led1);
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t status_handler(httpd_req_t *req) {
    char resp[50];
    int state = controller_get_status(global_ctrl);
    sprintf(resp, "{\"led1\": %d}", state);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ----------- WEB UI -----------

esp_err_t root_handler(httpd_req_t *req) {
    const char *html =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>"
        "body { font-family: Arial; text-align: center; margin-top: 50px; }"
        "button { font-size: 30px; padding: 20px; border-radius: 10px; }"
        "</style>"
        "</head>"
        "<body>"
        "<h1>Control LED</h1>"
        "<button onclick='toggle()'>Encender / Apagar</button>"
        "<p id='status'>Estado: ...</p>"
        "<script>"
        "function toggle() {"
        "  fetch('/toggle').then(() => loadStatus());"
        "}"
        "function loadStatus() {"
        "  fetch('/status')"
        "    .then(r => r.json())"
        "    .then(data => {"
        "      document.getElementById('status').innerText = 'Estado: ' + (data.led1 ? 'ENCENDIDO' : 'APAGADO');"
        "    });"
        "}"
        "loadStatus();"
        "</script>"
        "</body>"
        "</html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ----------- SERVER -----------

void start_webserver(Controller *controller) {
    global_ctrl = controller;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {

        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler
        };

        httpd_uri_t toggle_uri = {
            .uri = "/toggle",
            .method = HTTP_GET,
            .handler = toggle_handler
        };

        httpd_uri_t status_uri = {
            .uri = "/status",
            .method = HTTP_GET,
            .handler = status_handler
        };

        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &toggle_uri);
        httpd_register_uri_handler(server, &status_uri);
    }
}

// ----------- TASK -----------

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void webserver_task(void *pvParameters) {
    Controller *controller = (Controller *)pvParameters;

    start_webserver(controller);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
