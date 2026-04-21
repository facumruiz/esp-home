#include "webserver.h"
#include "led_strip_ws.h"
#include "lux_sensor.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "WEBSERVER";
static Controller *global_ctrl;

// ─── API LED GPIO ─────────────────────────────────────────────────────────────

esp_err_t toggle_handler(httpd_req_t *req) {
    led_toggle(global_ctrl->led1);
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t status_handler(httpd_req_t *req) {
    char resp[50];
    sprintf(resp, "{\"led1\": %d}", controller_get_status(global_ctrl));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ─── API tira WS2812 ─────────────────────────────────────────────────────────

esp_err_t strip_status_handler(httpd_req_t *req) {
    LedStrip *s = global_ctrl->strip1;
    char resp[128];
    snprintf(resp, sizeof(resp),
        "{\"state\":%s,\"r\":%d,\"g\":%d,\"b\":%d,\"brightness\":%d,\"effect\":%d}",
        s->state ? "true" : "false",
        s->r, s->g, s->b, s->brightness, (int)s->effect);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t strip_set_handler(httpd_req_t *req) {
    char buf[256] = {0};
    int received = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Sin body");
        return ESP_FAIL;
    }
    cJSON *root = cJSON_ParseWithLength(buf, received);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON invalido");
        return ESP_FAIL;
    }
    LedStrip *s = global_ctrl->strip1;

    cJSON *state = cJSON_GetObjectItem(root, "state");
    if (cJSON_IsString(state))
        led_strip_ws_set_state(s, strcmp(state->valuestring, "ON") == 0);

    cJSON *brightness = cJSON_GetObjectItem(root, "brightness");
    if (cJSON_IsNumber(brightness)) {
        int val = brightness->valueint;
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        led_strip_ws_set_brightness(s, (uint8_t)val);
    }

    cJSON *color = cJSON_GetObjectItem(root, "color");
    if (cJSON_IsObject(color)) {
        cJSON *r = cJSON_GetObjectItem(color, "r");
        cJSON *g = cJSON_GetObjectItem(color, "g");
        cJSON *b = cJSON_GetObjectItem(color, "b");
        if (cJSON_IsNumber(r) && cJSON_IsNumber(g) && cJSON_IsNumber(b))
            led_strip_ws_set_color(s,
                (uint8_t)r->valueint,
                (uint8_t)g->valueint,
                (uint8_t)b->valueint);
    }

    cJSON *effect = cJSON_GetObjectItem(root, "effect");
    if (cJSON_IsString(effect)) {
        strip_effect_t ef = STRIP_EFFECT_NONE;
        if      (strcmp(effect->valuestring, "blink")   == 0) ef = STRIP_EFFECT_BLINK;
        else if (strcmp(effect->valuestring, "fade")    == 0) ef = STRIP_EFFECT_FADE;
        else if (strcmp(effect->valuestring, "rainbow") == 0) ef = STRIP_EFFECT_RAINBOW;
        led_strip_ws_set_effect(s, ef);
    }

    if (s->effect == STRIP_EFFECT_NONE)
        led_strip_ws_apply(s);

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ─── API sensor lux ───────────────────────────────────────────────────────────

esp_err_t lux_status_handler(httpd_req_t *req) {
    lux_sensor_read(global_ctrl->lux1);
    char resp[64];
    snprintf(resp, sizeof(resp),
        "{\"dark\":%s,\"auto\":%s}",
        lux_sensor_is_dark(global_ctrl->lux1) ? "true" : "false",
        global_ctrl->auto_mode ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t lux_auto_handler(httpd_req_t *req) {
    global_ctrl->auto_mode = !global_ctrl->auto_mode;
    char resp[32];
    snprintf(resp, sizeof(resp),
        "{\"auto\":%s}",
        global_ctrl->auto_mode ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ─── UI HTML ─────────────────────────────────────────────────────────────────

esp_err_t root_handler(httpd_req_t *req) {
    const char *html =
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Domotica M2</title>"
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;padding:20px}"
        "h1{text-align:center;margin-bottom:24px;color:#e94560;font-size:1.6em}"
        ".card{background:#16213e;border-radius:16px;padding:20px;margin-bottom:16px}"
        ".card h2{font-size:.85em;color:#a8a8b3;margin-bottom:16px;"
        "  text-transform:uppercase;letter-spacing:1px}"
        ".row{display:flex;gap:10px;flex-wrap:wrap;justify-content:center;margin-bottom:12px}"
        "button{padding:12px 20px;border:none;border-radius:10px;"
        "  font-size:.95em;cursor:pointer;font-weight:bold;transition:opacity .2s}"
        "button:active{opacity:.7}"
        ".btn-on {background:#27ae60;color:#fff}"
        ".btn-off{background:#e74c3c;color:#fff}"
        ".btn-fx {background:#2980b9;color:#fff;flex:1;min-width:80px}"
        ".btn-fx.active{outline:3px solid #fff}"
        ".btn-auto{width:100%;padding:14px;font-size:1em;"
        "  background:#8e44ad;color:#fff;border-radius:10px;border:none;cursor:pointer}"
        ".btn-auto.on{background:#27ae60}"
        "label{display:block;margin-bottom:6px;font-size:.85em;color:#a8a8b3}"
        "input[type=range]{width:100%;accent-color:#e94560;margin-bottom:4px}"
        "input[type=color]{width:100%;height:44px;border:none;"
        "  border-radius:8px;cursor:pointer;background:none}"
        ".lux-icon{text-align:center;font-size:3em;margin:10px 0}"
        ".lux-label{text-align:center;font-size:.9em;color:#a8a8b3}"
        "#statusBar{text-align:center;padding:8px;border-radius:8px;"
        "  background:#0f3460;font-size:.85em;margin-top:8px}"
        ".hint{text-align:center;font-size:.75em;color:#555;margin-top:8px}"
        "</style></head><body>"

        "<h1>&#128161; Domotica M2</h1>"

        // ── Card sensor ──
        "<div class='card'>"
        "<h2>Sensor Luminosidad (LM393)</h2>"
        "<div class='lux-icon' id='luxIcon'>&#9728;</div>"
        "<div class='lux-label' id='luxLabel'>Cargando...</div>"
        "<br>"
        "<button class='btn-auto' id='btnAuto' onclick='toggleAuto()'>"
        "Modo automatico: <span id='autoLabel'>OFF</span>"
        "</button>"
        "<p class='hint'>Oscuro &rarr; tira ON &nbsp;|&nbsp; Luz &rarr; tira OFF</p>"
        "</div>"

        // ── Card tira ──
        "<div class='card'>"
        "<h2>Tira WS2812B</h2>"
        "<div class='row'>"
        "<button class='btn-on'  onclick='setState(\"ON\")'>ON</button>"
        "<button class='btn-off' onclick='setState(\"OFF\")'>OFF</button>"
        "</div>"
        "<label>Color</label>"
        "<input type='color' id='colorPick' value='#ff6400' oninput='sendColor()'>"
        "<label style='margin-top:12px'>Brillo: <span id='briVal'>128</span></label>"
        "<input type='range' min='1' max='255' value='128' id='briRange'"
        "  oninput='document.getElementById(\"briVal\").innerText=this.value'"
        "  onchange='sendBrightness()'>"
        "<label style='margin-top:12px'>Efectos</label>"
        "<div class='row'>"
        "<button class='btn-fx' id='fx-none'    onclick='setEffect(\"none\")'>Estatico</button>"
        "<button class='btn-fx' id='fx-blink'   onclick='setEffect(\"blink\")'>Blink</button>"
        "<button class='btn-fx' id='fx-fade'    onclick='setEffect(\"fade\")'>Fade</button>"
        "<button class='btn-fx' id='fx-rainbow' onclick='setEffect(\"rainbow\")'>Rainbow</button>"
        "</div>"
        "</div>"

        // ── Card GPIO ──
        "<div class='card'>"
        "<h2>LED Simple (GPIO)</h2>"
        "<div class='row'>"
        "<button class='btn-on' onclick='fetch(\"/toggle\").then(()=>loadGpio())'>Toggle</button>"
        "</div>"
        "<p id='gpioStatus' style='text-align:center;font-size:.85em;color:#a8a8b3'>...</p>"
        "</div>"

        "<div id='statusBar'>Listo</div>"

        "<script>"
        "function setStatus(m){document.getElementById('statusBar').innerText=m;}"
        "function post(u,b){"
        "  return fetch(u,{method:'POST',"
        "    headers:{'Content-Type':'application/json'},"
        "    body:JSON.stringify(b)});}"
        "function hexToRgb(h){"
        "  return{r:parseInt(h.slice(1,3),16),"
        "         g:parseInt(h.slice(3,5),16),"
        "         b:parseInt(h.slice(5,7),16)};}"

        "function loadLux(){"
        "  fetch('/lux/status').then(r=>r.json()).then(d=>{"
        "    var icon=document.getElementById('luxIcon');"
        "    var lbl =document.getElementById('luxLabel');"
        "    icon.innerHTML = d.dark ? '&#9790;' : '&#9728;';"
        "    lbl.innerText  = d.dark ? 'OSCURO'  : 'HAY LUZ';"
        "    var btn=document.getElementById('btnAuto');"
        "    var al =document.getElementById('autoLabel');"
        "    if(d.auto){btn.classList.add('on');al.innerText='ON';}"
        "    else{btn.classList.remove('on');al.innerText='OFF';}"
        "  });}"

        "function toggleAuto(){"
        "  fetch('/lux/auto').then(r=>r.json()).then(d=>{"
        "    var btn=document.getElementById('btnAuto');"
        "    var al =document.getElementById('autoLabel');"
        "    if(d.auto){btn.classList.add('on');al.innerText='ON';"
        "      setStatus('Modo auto: ON');}"
        "    else{btn.classList.remove('on');al.innerText='OFF';"
        "      setStatus('Modo auto: OFF');}"
        "  });}"

        "function setState(s){"
        "  post('/strip/set',{state:s}).then(()=>setStatus('Tira: '+s));}"
        "function sendColor(){"
        "  var rgb=hexToRgb(document.getElementById('colorPick').value);"
        "  post('/strip/set',{color:rgb}).then(()=>setStatus('Color aplicado'));}"
        "function sendBrightness(){"
        "  var b=parseInt(document.getElementById('briRange').value);"
        "  post('/strip/set',{brightness:b}).then(()=>setStatus('Brillo: '+b));}"
        "function setEffect(ef){"
        "  document.querySelectorAll('.btn-fx')"
        "    .forEach(b=>b.classList.remove('active'));"
        "  document.getElementById('fx-'+ef).classList.add('active');"
        "  post('/strip/set',{effect:ef}).then(()=>setStatus('Efecto: '+ef));}"

        "function loadGpio(){"
        "  fetch('/status').then(r=>r.json()).then(d=>{"
        "    document.getElementById('gpioStatus').innerText="
        "      'Estado: '+(d.led1?'ENCENDIDO':'APAGADO');});}"

        "loadLux(); loadGpio();"
        "setInterval(loadLux, 3000);"
        "</script></body></html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ─── Server ──────────────────────────────────────────────────────────────────

void start_webserver(Controller *controller) {
    global_ctrl = controller;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Error al iniciar webserver");
        return;
    }

    httpd_uri_t uris[] = {
        { .uri="/",            .method=HTTP_GET,  .handler=root_handler         },
        { .uri="/toggle",      .method=HTTP_GET,  .handler=toggle_handler       },
        { .uri="/status",      .method=HTTP_GET,  .handler=status_handler       },
        { .uri="/strip/status",.method=HTTP_GET,  .handler=strip_status_handler },
        { .uri="/strip/set",   .method=HTTP_POST, .handler=strip_set_handler    },
        { .uri="/lux/status",  .method=HTTP_GET,  .handler=lux_status_handler   },
        { .uri="/lux/auto",    .method=HTTP_GET,  .handler=lux_auto_handler     },
    };

    for (int i = 0; i < sizeof(uris)/sizeof(uris[0]); i++) {
        httpd_register_uri_handler(server, &uris[i]);
    }

    ESP_LOGI(TAG, "Webserver iniciado");
}

// ─── Task ────────────────────────────────────────────────────────────────────

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void webserver_task(void *pvParameters) {
    Controller *controller = (Controller *)pvParameters;
    start_webserver(controller);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
