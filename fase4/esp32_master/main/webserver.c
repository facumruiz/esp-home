#include "config.h"
#include "webserver.h"
#include "led_strip_ws.h"
#include "lux_sensor.h"
#include "espnow_master.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "WEBSERVER";
static Controller *global_ctrl;

// ─── LED GPIO ────────────────────────────────────────────────
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

// ─── Strip genérico ──────────────────────────────────────────
static esp_err_t strip_set(httpd_req_t *req, LedStrip *s) {
    char buf[256] = {0};
    int received = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (received <= 0) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Sin body"); return ESP_FAIL; }
    cJSON *root = cJSON_ParseWithLength(buf, received);
    if (!root) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON invalido"); return ESP_FAIL; }

    cJSON *state = cJSON_GetObjectItem(root, "state");
    if (cJSON_IsString(state)) led_strip_ws_set_state(s, strcmp(state->valuestring, "ON") == 0);

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
            led_strip_ws_set_color(s, (uint8_t)r->valueint, (uint8_t)g->valueint, (uint8_t)b->valueint);
    }

    cJSON *effect = cJSON_GetObjectItem(root, "effect");
    if (cJSON_IsString(effect)) {
        strip_effect_t ef = STRIP_EFFECT_NONE;
        if      (strcmp(effect->valuestring, "blink")   == 0) ef = STRIP_EFFECT_BLINK;
        else if (strcmp(effect->valuestring, "fade")    == 0) ef = STRIP_EFFECT_FADE;
        else if (strcmp(effect->valuestring, "rainbow") == 0) ef = STRIP_EFFECT_RAINBOW;
        led_strip_ws_set_effect(s, ef);
    }

    if (s->effect == STRIP_EFFECT_NONE) led_strip_ws_apply(s);
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t strip_status(httpd_req_t *req, LedStrip *s) {
    char resp[128];
    snprintf(resp, sizeof(resp),
        "{\"state\":%s,\"r\":%d,\"g\":%d,\"b\":%d,\"brightness\":%d,\"effect\":%d}",
        s->state ? "true" : "false",
        s->r, s->g, s->b, s->brightness, (int)s->effect);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ─── Tira 1 ──────────────────────────────────────────────────
esp_err_t strip1_status_handler(httpd_req_t *req) { return strip_status(req, global_ctrl->strip1); }
esp_err_t strip1_set_handler(httpd_req_t *req)    { return strip_set(req, global_ctrl->strip1); }

// ─── Tira 2 ──────────────────────────────────────────────────
esp_err_t strip2_status_handler(httpd_req_t *req) { return strip_status(req, global_ctrl->strip2); }
esp_err_t strip2_set_handler(httpd_req_t *req)    { return strip_set(req, global_ctrl->strip2); }

// ─── Sensor lux ──────────────────────────────────────────────
esp_err_t lux_status_handler(httpd_req_t *req) {
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"dark\":%s,\"auto\":%s}",
        g_node_status.dark ? "true" : "false",
        global_ctrl->auto_mode ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
esp_err_t lux_auto_handler(httpd_req_t *req) {
    global_ctrl->auto_mode = !global_ctrl->auto_mode;
    char resp[32];
    snprintf(resp, sizeof(resp), "{\"auto\":%s}", global_ctrl->auto_mode ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ─── Nodo ────────────────────────────────────────────────────
esp_err_t node_toggle_handler(httpd_req_t *req) {
    global_ctrl->node_enabled = !global_ctrl->node_enabled;
    char resp[32];
    snprintf(resp, sizeof(resp), "{\"enabled\":%s}", global_ctrl->node_enabled ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
esp_err_t node_status_handler(httpd_req_t *req) {
    bool online = espnow_node_is_online();
    char resp[128];
    if (global_ctrl->node_enabled && online) {
        snprintf(resp, sizeof(resp),
            "{\"enabled\":true,\"online\":true,\"dark\":%s,\"motion\":%s,\"pir_enabled\":%s,\"auto\":%s}",
            g_node_status.dark   ? "true" : "false",
            g_node_status.motion ? "true" : "false",
            global_ctrl->pir_enabled ? "true" : "false",
            global_ctrl->auto_mode   ? "true" : "false");
    } else {
        snprintf(resp, sizeof(resp),
            "{\"enabled\":%s,\"online\":false,\"pir_enabled\":%s,\"auto\":%s}",
            global_ctrl->node_enabled ? "true" : "false",
            global_ctrl->pir_enabled  ? "true" : "false",
            global_ctrl->auto_mode    ? "true" : "false");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
esp_err_t pir_toggle_handler(httpd_req_t *req) {
    global_ctrl->pir_enabled = !global_ctrl->pir_enabled;
    char resp[32];
    snprintf(resp, sizeof(resp), "{\"pir_enabled\":%s}", global_ctrl->pir_enabled ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
esp_err_t pir_status_handler(httpd_req_t *req) {
    char resp[128];
    snprintf(resp, sizeof(resp),
        "{\"motion\":%s,\"keep_on\":%s,\"hold_remaining_ms\":%lu,\"hold_total_ms\":%d,\"led_on\":%s}",
        g_node_status.motion             ? "true" : "false",
        global_ctrl->pir_keep_on         ? "true" : "false",
        (unsigned long)global_ctrl->pir_hold_remaining_ms,
        MOTION_HOLD_MS,
        global_ctrl->strip2->state       ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ─── UI HTML ─────────────────────────────────────────────────
esp_err_t root_handler(httpd_req_t *req) {
    const char *html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESP-HOME</title>"
    "<style>"
    ":root{--bg:#0f0f1a;--card:#1a1a2e;--card2:#16213e;--accent:#7c4dff;"
    "--accent2:#00e5ff;--on:#00c853;--off:#ff1744;--text:#e0e0e0;--sub:#9e9e9e}"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:'Segoe UI',Arial,sans-serif;background:var(--bg);color:var(--text);padding:16px;max-width:520px;margin:0 auto}"
    "h1{text-align:center;margin-bottom:20px;font-size:1.4em;font-weight:300;letter-spacing:2px;color:var(--accent2)}"
    ".card{background:var(--card);border-radius:20px;padding:16px;margin-bottom:14px;box-shadow:0 4px 20px rgba(0,0,0,.4)}"
    ".card-title{font-size:.7em;color:var(--sub);text-transform:uppercase;letter-spacing:2px;margin-bottom:12px;display:flex;align-items:center;gap:8px}"
    ".card-title::before{content:'';display:block;width:3px;height:14px;background:var(--accent);border-radius:2px}"
    ".two-col{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:14px}"
    ".strip-card{background:var(--card);border-radius:20px;padding:14px;box-shadow:0 4px 20px rgba(0,0,0,.4)}"
    ".strip-card .card-title::before{background:var(--accent2)}"
    ".state-dot{width:10px;height:10px;border-radius:50%;display:inline-block;margin-right:6px;flex-shrink:0}"
    ".state-dot.on{background:var(--on);box-shadow:0 0 6px var(--on)}"
    ".state-dot.off{background:var(--sub)}"
    ".state-label{font-size:.95em;font-weight:600;display:flex;align-items:center;margin-bottom:10px}"
    ".row{display:flex;gap:6px;margin-bottom:10px}"
    ".btn{flex:1;padding:10px 6px;border:none;border-radius:10px;font-size:.8em;font-weight:600;cursor:pointer;transition:all .2s}"
    ".btn:active{transform:scale(.95)}"
    ".btn-on{background:rgba(0,200,83,.2);color:var(--on);border:1px solid rgba(0,200,83,.3)}"
    ".btn-off{background:rgba(255,23,68,.2);color:var(--off);border:1px solid rgba(255,23,68,.3)}"
    ".btn-fx{background:rgba(0,229,255,.1);color:var(--accent2);border:1px solid rgba(0,229,255,.2);font-size:.72em;padding:8px 4px}"
    ".btn-fx.active{background:rgba(0,229,255,.25);border-color:var(--accent2)}"
    ".toggle{width:100%;padding:11px;border:none;border-radius:11px;font-size:.85em;font-weight:600;cursor:pointer;transition:all .2s;margin-bottom:7px}"
    ".toggle.inactive{background:rgba(124,77,255,.15);color:var(--accent);border:1px solid rgba(124,77,255,.3)}"
    ".toggle.active{background:rgba(0,200,83,.15);color:var(--on);border:1px solid rgba(0,200,83,.3)}"
    ".chip{display:inline-block;padding:2px 8px;border-radius:20px;font-size:.68em;font-weight:700}"
    ".chip.on{background:rgba(0,200,83,.15);color:var(--on);border:1px solid rgba(0,200,83,.3)}"
    ".chip.off{background:rgba(255,23,68,.15);color:var(--off);border:1px solid rgba(255,23,68,.3)}"
    ".chip.motion{background:rgba(255,145,0,.2);color:#ff9100;border:1px solid rgba(255,145,0,.4)}"
    "label{font-size:.72em;color:var(--sub);display:block;margin-bottom:4px;margin-top:8px}"
    "input[type=range]{width:100%;accent-color:var(--accent);margin-bottom:2px}"
    ".color-row{display:flex;align-items:center;gap:8px;margin-bottom:8px}"
    ".color-preview{width:32px;height:32px;border-radius:8px;background:#ff6400;flex-shrink:0;border:1px solid rgba(255,255,255,.1)}"
    ".color-wheel-wrap{position:relative;margin:6px auto;width:fit-content}"
    "canvas{display:block;border-radius:50%;cursor:crosshair}"
    ".sensor-row{display:flex;justify-content:space-between;align-items:center;margin-bottom:8px}"
    ".signal-dot{width:8px;height:8px;border-radius:50%;display:inline-block}"
    ".signal-dot.online{background:var(--on);box-shadow:0 0 5px var(--on)}"
    ".signal-dot.offline{background:var(--off)}"
    ".signal-dot.disabled{background:var(--sub)}"
    ".pir-pill{display:flex;align-items:center;gap:8px;padding:8px 12px;border-radius:12px;margin-bottom:8px;border:1px solid rgba(255,255,255,.07)}"
    ".pir-dot{width:9px;height:9px;border-radius:50%;flex-shrink:0}"
    ".pir-dot.active{background:#ff9100;box-shadow:0 0 7px #ff9100}"
    ".pir-dot.inactive{background:var(--sub)}"
    ".sw-row{display:flex;align-items:center;justify-content:space-between;padding:11px 0;border-bottom:1px solid rgba(255,255,255,.05)}"
    ".sw-row:last-child{border-bottom:none}"
    ".sw-info{display:flex;flex-direction:column;gap:3px}"
    ".sw-label{font-size:.88em;font-weight:500;color:var(--text)}"
    ".sw-sub{font-size:.72em;color:var(--sub)}"
    ".ios-sw{position:relative;width:48px;height:28px;flex-shrink:0}"
    ".ios-sw input{opacity:0;width:0;height:0}"
    ".ios-track{position:absolute;inset:0;background:rgba(255,255,255,.12);border-radius:28px;cursor:pointer;transition:.3s}"
    ".ios-sw input:checked+.ios-track{background:#34c759}"
    ".ios-track::before{content:'';position:absolute;width:22px;height:22px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.3s;box-shadow:0 2px 5px rgba(0,0,0,.3)}"
    ".ios-sw input:checked+.ios-track::before{transform:translateX(20px)}"
    "#toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%) translateY(80px);"
    "background:var(--card2);color:var(--text);padding:10px 20px;border-radius:20px;"
    "font-size:.8em;transition:transform .3s;box-shadow:0 4px 20px rgba(0,0,0,.5);"
    "border:1px solid rgba(255,255,255,.1);pointer-events:none}"
    "#toast.show{transform:translateX(-50%) translateY(0)}"
    "</style></head><body>"
    "<h1>&#9964; ESP-HOME</h1>"

    // ── Dos cards tiras ──
    "<div class='two-col'>"

    // Tira 1
    "<div class='strip-card'>"
    "<div class='card-title'>Tira 1 · GPIO27 - LDR</div>"
    "<div class='state-label'><span class='state-dot off' id='s1dot'></span><span id='s1state'>OFF</span></div>"
    "<div class='row'>"
    "<button class='btn btn-on' onclick='s1State(\"ON\")'>ON</button>"
    "<button class='btn btn-off' onclick='s1State(\"OFF\")'>OFF</button>"
    "</div>"
    "<label>Color</label>"
    "<div class='color-row'>"
    "<div class='color-preview' id='s1preview'></div>"
    "<input type='color' id='s1color' value='#ff6400' style='flex:1;height:32px;border:none;border-radius:8px;cursor:pointer;background:none' oninput='s1Color()'>"
    "</div>"
    "<label>Brillo <span id='s1briVal'>128</span></label>"
    "<input type='range' min='1' max='255' value='128' id='s1bri' oninput='document.getElementById(\"s1briVal\").innerText=this.value' onchange='s1Bri()'>"
    "<label>Efecto</label>"
    "<div class='row'>"
    "<button class='btn btn-fx active' id='s1fx-none'    onclick='s1Fx(\"none\")'>&#9632;</button>"
    "<button class='btn btn-fx' id='s1fx-blink'   onclick='s1Fx(\"blink\")'>Blink</button>"
    "<button class='btn btn-fx' id='s1fx-fade'    onclick='s1Fx(\"fade\")'>Fade</button>"
    "<button class='btn btn-fx' id='s1fx-rainbow' onclick='s1Fx(\"rainbow\")'>&#127752;</button>"
    "</div>"
    "</div>"

    // Tira 2
    "<div class='strip-card'>"
    "<div class='card-title'>Tira 2 · GPIO13 - PIR</div>"
    "<div class='state-label'><span class='state-dot off' id='s2dot'></span><span id='s2state'>OFF</span></div>"
    "<div class='row'>"
    "<button class='btn btn-on' onclick='s2State(\"ON\")'>ON</button>"
    "<button class='btn btn-off' onclick='s2State(\"OFF\")'>OFF</button>"
    "</div>"
    "<label>Color</label>"
    "<div class='color-row'>"
    "<div class='color-preview' id='s2preview'></div>"
    "<input type='color' id='s2color' value='#ffc864' style='flex:1;height:32px;border:none;border-radius:8px;cursor:pointer;background:none' oninput='s2Color()'>"
    "</div>"
    "<label>Brillo <span id='s2briVal'>200</span></label>"
    "<input type='range' min='1' max='255' value='200' id='s2bri' oninput='document.getElementById(\"s2briVal\").innerText=this.value' onchange='s2Bri()'>"
    "<label>Efecto</label>"
    "<div class='row'>"
    "<button class='btn btn-fx active' id='s2fx-none'    onclick='s2Fx(\"none\")'>&#9632;</button>"
    "<button class='btn btn-fx' id='s2fx-blink'   onclick='s2Fx(\"blink\")'>Blink</button>"
    "<button class='btn btn-fx' id='s2fx-fade'    onclick='s2Fx(\"fade\")'>Fade</button>"
    "<button class='btn btn-fx' id='s2fx-rainbow' onclick='s2Fx(\"rainbow\")'>&#127752;</button>"
    "</div>"
    "</div>"
    "</div>"

    // ── Card Nodo ──
    "<div class='card'>"
    "<div class='card-title'>Nodo ESP32 &nbsp;"
    "<span class='signal-dot disabled' id='signalDot'></span>&nbsp;"
    "<span id='chipNode' class='chip off'>--</span>"
    "</div>"
    "<div class='sensor-row'>"
    "<div>"
    "<div style='font-size:.8em;color:var(--sub)'>Luz</div>"
    "<div style='font-weight:600' id='nodeLux'>--</div>"
    "</div>"
    "<div style='text-align:right'>"
    "<div style='font-size:.8em;color:var(--sub)'>PIR</div>"
    "<div id='nodeMotion' class='chip off'>--</div>"
    "</div>"
    "</div>"
    "<div class='pir-pill' id='pirPill'>"
    "<div class='pir-dot inactive' id='pirDot'></div>"
    "<span id='pirLabel' style='font-size:.85em;color:var(--sub)'>Sin movimiento</span>"
    "<span style='margin-left:auto;font-size:.75em;color:var(--sub)' id='pirTimer'></span>"
    "</div>"
    "<div class='sw-row'><div class='sw-info'><span class='sw-label'>Sensor PIR</span><span class='sw-sub'>Tira 2 reacciona al movimiento</span></div><label class='ios-sw'><input type='checkbox' id='swPir' checked onchange='togglePir()'><span class='ios-track'></span></label></div>"
    "<div class='sw-row'><div class='sw-info'><span class='sw-label'>Modo Auto Luz</span><span class='sw-sub'>Tira 1 responde al sensor de luz</span></div><label class='ios-sw'><input type='checkbox' id='swAuto' onchange='toggleAuto()'><span class='ios-track'></span></label></div>"
    "<div class='sw-row'><div class='sw-info'><span class='sw-label'>Nodo ESP-NOW</span><span class='sw-sub'>Habilita la conexion con el nodo</span></div><label class='ios-sw'><input type='checkbox' id='swNode' checked onchange='toggleNode()'><span class='ios-track'></span></label></div>"
    "</div>"

    // ── Card GPIO ──
    "<div class='card'>"
    "<div class='card-title'>LED Simple GPIO2</div>"
    "<div class='row'>"
    "<button class='btn btn-on' onclick='fetch(\"/toggle\").then(()=>loadGpio())'>Toggle</button>"
    "</div>"
    "<div style='text-align:center;font-size:.85em;color:var(--sub)' id='gpioStatus'>...</div>"
    "</div>"

    "<div id='toast'></div>"
    "<script>"
    "function toast(m,d=1800){var t=document.getElementById('toast');t.innerText=m;t.classList.add('show');setTimeout(()=>t.classList.remove('show'),d);}"
    "function post(u,b){return fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});}"
    "function hexToRgb(h){return{r:parseInt(h.slice(1,3),16),g:parseInt(h.slice(3,5),16),b:parseInt(h.slice(5,7),16)};}"
    "function rgbToHex(r,g,b){return'#'+[r,g,b].map(x=>x.toString(16).padStart(2,'0')).join('');}"

    // Tira 1
    "function loadS1(){fetch('/strip1/status').then(r=>r.json()).then(d=>{"
    "  var on=d.state;"
    "  document.getElementById('s1dot').className='state-dot '+(on?'on':'off');"
    "  document.getElementById('s1state').innerText=on?'ON':'OFF';"
    "  var hex=rgbToHex(d.r,d.g,d.b);"
    "  document.getElementById('s1color').value=hex;"
    "  document.getElementById('s1preview').style.background=hex;"
    "  document.getElementById('s1bri').value=d.brightness;"
    "  document.getElementById('s1briVal').innerText=d.brightness;"
    "});}"
    "function s1State(s){post('/strip1/set',{state:s}).then(()=>{toast('Tira 1: '+s);loadS1();});}"
    "function s1Color(){var rgb=hexToRgb(document.getElementById('s1color').value);document.getElementById('s1preview').style.background=document.getElementById('s1color').value;post('/strip1/set',{color:rgb}).then(()=>toast('Color tira 1'));}"
    "function s1Bri(){var b=parseInt(document.getElementById('s1bri').value);post('/strip1/set',{brightness:b}).then(()=>toast('Brillo tira 1: '+b));}"
    "function s1Fx(ef){document.querySelectorAll('[id^=s1fx]').forEach(b=>b.classList.remove('active'));document.getElementById('s1fx-'+ef).classList.add('active');post('/strip1/set',{effect:ef}).then(()=>toast('Efecto tira 1: '+ef));}"

    // Tira 2
    "function loadS2(){fetch('/strip2/status').then(r=>r.json()).then(d=>{"
    "  var on=d.state;"
    "  document.getElementById('s2dot').className='state-dot '+(on?'on':'off');"
    "  document.getElementById('s2state').innerText=on?'ON':'OFF';"
    "  var hex=rgbToHex(d.r,d.g,d.b);"
    "  document.getElementById('s2color').value=hex;"
    "  document.getElementById('s2preview').style.background=hex;"
    "  document.getElementById('s2bri').value=d.brightness;"
    "  document.getElementById('s2briVal').innerText=d.brightness;"
    "});}"
    "function s2State(s){post('/strip2/set',{state:s}).then(()=>{toast('Tira 2: '+s);loadS2();});}"
    "function s2Color(){var rgb=hexToRgb(document.getElementById('s2color').value);document.getElementById('s2preview').style.background=document.getElementById('s2color').value;post('/strip2/set',{color:rgb}).then(()=>toast('Color tira 2'));}"
    "function s2Bri(){var b=parseInt(document.getElementById('s2bri').value);post('/strip2/set',{brightness:b}).then(()=>toast('Brillo tira 2: '+b));}"
    "function s2Fx(ef){document.querySelectorAll('[id^=s2fx]').forEach(b=>b.classList.remove('active'));document.getElementById('s2fx-'+ef).classList.add('active');post('/strip2/set',{effect:ef}).then(()=>toast('Efecto tira 2: '+ef));}"

    // Nodo
    "function loadNode(){fetch('/node/status').then(r=>r.json()).then(d=>{"
    "  var online=d.online&&d.enabled;"
    "  document.getElementById('signalDot').className='signal-dot '+(d.enabled?(online?'online':'offline'):'disabled');"
    "  var chip=document.getElementById('chipNode');"
    "  chip.innerText=d.enabled?(online?'ON':'OFFLINE'):'OFF';"
    "  chip.className='chip '+(online?'on':'off');"
    "  if(online){"
    "    document.getElementById('nodeLux').innerText=d.dark?'OSCURO ☾':'LUZ ☀';"
    "    var mc=document.getElementById('nodeMotion');"
    "    mc.innerText=d.motion?'MOVIMIENTO':'QUIETO';"
    "    mc.className='chip '+(d.motion?'motion':'off');"
    "    var pd=document.getElementById('pirDot');"
    "    var pl=document.getElementById('pirLabel');"
    "    pd.className='pir-dot '+(d.motion?'active':'inactive');"
    "    pl.innerText=d.motion?'Movimiento detectado':'Sin movimiento';"
    "  }"
    "  var sp=document.getElementById('swPir');if(sp)sp.checked=d.pir_enabled;"
    "  var sa=document.getElementById('swAuto');if(sa)sa.checked=d.auto;"
    "  var sn=document.getElementById('swNode');if(sn)sn.checked=d.enabled;"
    "});}"
    "function toggleNode(){fetch('/node/toggle').then(r=>r.json()).then(d=>{toast('Nodo: '+(d.enabled?'ON':'OFF'));loadNode();});}"
    "function togglePir(){fetch('/pir/toggle').then(r=>r.json()).then(d=>{toast('PIR: '+(d.pir_enabled?'ON':'OFF'));loadNode();});}"
    "function toggleAuto(){fetch('/lux/auto').then(r=>r.json()).then(d=>{toast('Auto: '+(d.auto?'ON':'OFF'));loadNode();});}"

    "function loadGpio(){fetch('/status').then(r=>r.json()).then(d=>{document.getElementById('gpioStatus').innerText='Estado: '+(d.led1?'ENCENDIDO':'APAGADO');});}"

    "loadS1();loadS2();loadNode();loadGpio();"
    "setInterval(loadS1,4000);"
    "setInterval(loadS2,4000);"
    "setInterval(loadNode,2000);"
    "setInterval(loadGpio,6000);"
    "</script></body></html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void start_webserver(Controller *controller) {
    global_ctrl = controller;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Error al iniciar webserver");
        return;
    }
    httpd_uri_t uris[] = {
        { .uri="/",              .method=HTTP_GET,  .handler=root_handler          },
        { .uri="/toggle",        .method=HTTP_GET,  .handler=toggle_handler        },
        { .uri="/status",        .method=HTTP_GET,  .handler=status_handler        },
        { .uri="/strip1/status", .method=HTTP_GET,  .handler=strip1_status_handler },
        { .uri="/strip1/set",    .method=HTTP_POST, .handler=strip1_set_handler    },
        { .uri="/strip2/status", .method=HTTP_GET,  .handler=strip2_status_handler },
        { .uri="/strip2/set",    .method=HTTP_POST, .handler=strip2_set_handler    },
        { .uri="/lux/status",    .method=HTTP_GET,  .handler=lux_status_handler    },
        { .uri="/lux/auto",      .method=HTTP_GET,  .handler=lux_auto_handler      },
        { .uri="/node/status",   .method=HTTP_GET,  .handler=node_status_handler   },
        { .uri="/node/toggle",   .method=HTTP_GET,  .handler=node_toggle_handler   },
        { .uri="/pir/status",    .method=HTTP_GET,  .handler=pir_status_handler    },
        { .uri="/pir/toggle",    .method=HTTP_GET,  .handler=pir_toggle_handler    },
    };
    for (int i = 0; i < sizeof(uris)/sizeof(uris[0]); i++) {
        httpd_register_uri_handler(server, &uris[i]);
    }
    ESP_LOGI(TAG, "Webserver iniciado");
}

void webserver_task(void *pvParameters) {
    Controller *controller = (Controller *)pvParameters;
    start_webserver(controller);
    while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}
