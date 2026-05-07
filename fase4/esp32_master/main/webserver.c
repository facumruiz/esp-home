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
    if (received <= 0) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Sin body"); return ESP_FAIL; }
    cJSON *root = cJSON_ParseWithLength(buf, received);
    if (!root) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON invalido"); return ESP_FAIL; }
    LedStrip *s = global_ctrl->strip1;
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

esp_err_t lux_status_handler(httpd_req_t *req) {
    lux_sensor_read(global_ctrl->lux1);
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"dark\":%s,\"auto\":%s}",
        lux_sensor_is_dark(global_ctrl->lux1) ? "true" : "false",
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

// node/status solo devuelve enabled + auto — sensores solo si enabled
esp_err_t node_status_handler(httpd_req_t *req) {
    char resp[128];
    if (global_ctrl->node_enabled) {
        snprintf(resp, sizeof(resp),
            "{\"enabled\":true,\"auto\":%s,\"dark\":%s,\"motion\":%s}",
            global_ctrl->auto_mode    ? "true" : "false",
            g_node_status.dark        ? "true" : "false",
            g_node_status.motion      ? "true" : "false");
    } else {
        // nodo deshabilitado: solo estado de control, sin datos de sensores
        snprintf(resp, sizeof(resp),
            "{\"enabled\":false,\"auto\":%s}",
            global_ctrl->auto_mode ? "true" : "false");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t node_toggle_handler(httpd_req_t *req) {
    global_ctrl->node_enabled = !global_ctrl->node_enabled;
    char resp[32];
    snprintf(resp, sizeof(resp), "{\"enabled\":%s}",
        global_ctrl->node_enabled ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// pir/status retorna 204 si nodo deshabilitado — el JS lo detecta y para
esp_err_t pir_status_handler(httpd_req_t *req) {
    if (!global_ctrl->node_enabled) {
        httpd_resp_set_status(req, "204 No Content");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    char resp[128];
    snprintf(resp, sizeof(resp),
        "{\"motion\":%s,\"keep_on\":%s,\"hold_remaining_ms\":%lu,\"hold_total_ms\":%d,\"led_on\":%s}",
        g_node_status.motion            ? "true" : "false",
        global_ctrl->pir_keep_on        ? "true" : "false",
        (unsigned long)global_ctrl->pir_hold_remaining_ms,
        MOTION_HOLD_MS,
        led_get_state(global_ctrl->led1) ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t root_handler(httpd_req_t *req) {
    const char *html =
    "<!DOCTYPE html><html><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Domotica</title>"
    "<style>"
    ":root{--bg:#0f0f1a;--card:#1a1a2e;--card2:#16213e;--accent:#7c4dff;"
    "--accent2:#00e5ff;--on:#00c853;--off:#ff1744;--text:#e0e0e0;--sub:#9e9e9e;"
    "--motion:#ff9100}"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:'Segoe UI',Arial,sans-serif;background:var(--bg);color:var(--text);padding:16px;max-width:480px;margin:0 auto}"
    "h1{text-align:center;margin-bottom:20px;font-size:1.4em;font-weight:300;letter-spacing:2px;color:var(--accent2)}"
    ".card{background:var(--card);border-radius:20px;padding:18px;margin-bottom:14px;box-shadow:0 4px 20px rgba(0,0,0,.4)}"
    ".card-title{font-size:.7em;color:var(--sub);text-transform:uppercase;letter-spacing:2px;margin-bottom:14px;display:flex;align-items:center;gap:8px}"
    ".card-title::before{content:'';display:block;width:3px;height:14px;background:var(--accent);border-radius:2px}"
    ".sensor-row{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px}"
    ".sensor-icon{font-size:2.5em}"
    ".sensor-state{font-size:1.1em;font-weight:600}"
    ".sensor-state.dark{color:var(--accent2)}"
    ".sensor-state.light{color:#ffd740}"
    ".node-disabled-banner{text-align:center;padding:14px;color:var(--sub);"
    "  font-size:.85em;border:1px dashed rgba(255,255,255,.1);border-radius:12px;margin-bottom:10px}"
    ".pir-pill{display:flex;align-items:center;gap:10px;padding:10px 14px;"
    "  border-radius:14px;margin-bottom:10px;transition:all .3s;"
    "  border:1px solid rgba(255,255,255,.07)}"
    ".pir-pill.active{background:rgba(255,145,0,.12);border-color:rgba(255,145,0,.35)}"
    ".pir-pill.hold{background:rgba(255,145,0,.06);border-color:rgba(255,145,0,.18)}"
    ".pir-pill.inactive{background:rgba(255,255,255,.03)}"
    ".pir-dot{width:10px;height:10px;border-radius:50%;flex-shrink:0;transition:all .3s}"
    ".pir-dot.active{background:var(--motion);box-shadow:0 0 8px var(--motion)}"
    ".pir-dot.hold{background:#ffd740}"
    ".pir-dot.inactive{background:var(--sub)}"
    ".pir-label{font-size:.85em;font-weight:600}"
    ".pir-label.active{color:var(--motion)}"
    ".pir-label.hold{color:#ffd740}"
    ".pir-label.inactive{color:var(--sub)}"
    ".pir-sub{font-size:.7em;color:var(--sub);margin-top:2px}"
    ".hold-bar-wrap{width:100%;background:rgba(255,255,255,.06);border-radius:6px;height:6px;margin-bottom:10px;overflow:hidden}"
    ".hold-bar{height:6px;border-radius:6px;background:linear-gradient(90deg,#ff9100,#ffd740);transition:width .5s linear}"
    ".chip{display:inline-block;padding:3px 10px;border-radius:20px;font-size:.7em;font-weight:700;letter-spacing:1px}"
    ".chip.on{background:rgba(0,200,83,.15);color:var(--on);border:1px solid rgba(0,200,83,.3)}"
    ".chip.off{background:rgba(255,23,68,.15);color:var(--off);border:1px solid rgba(255,23,68,.3)}"
    ".chip.motion{background:rgba(255,145,0,.2);color:var(--motion);border:1px solid rgba(255,145,0,.4)}"
    ".chip.hold{background:rgba(255,215,0,.15);color:#ffd740;border:1px solid rgba(255,215,0,.3)}"
    ".chip.disabled{background:rgba(158,158,158,.1);color:var(--sub);border:1px solid rgba(158,158,158,.2)}"
    ".toggle{width:100%;padding:12px;border:none;border-radius:12px;font-size:.9em;font-weight:600;"
    "  cursor:pointer;transition:all .2s;letter-spacing:.5px;margin-bottom:8px}"
    ".toggle.inactive{background:rgba(124,77,255,.15);color:var(--accent);border:1px solid rgba(124,77,255,.3)}"
    ".toggle.active{background:rgba(0,200,83,.15);color:var(--on);border:1px solid rgba(0,200,83,.3)}"
    ".divider{height:1px;background:rgba(255,255,255,.05);margin:12px 0}"
    ".row{display:flex;gap:8px;margin-bottom:12px}"
    ".btn{flex:1;padding:12px;border:none;border-radius:12px;font-size:.9em;font-weight:600;cursor:pointer;transition:all .2s}"
    ".btn:active{transform:scale(.96)}"
    ".btn-on{background:rgba(0,200,83,.2);color:var(--on);border:1px solid rgba(0,200,83,.3)}"
    ".btn-off{background:rgba(255,23,68,.2);color:var(--off);border:1px solid rgba(255,23,68,.3)}"
    ".btn-fx{background:rgba(0,229,255,.1);color:var(--accent2);border:1px solid rgba(0,229,255,.2);font-size:.8em}"
    ".btn-fx.active{background:rgba(0,229,255,.25);border-color:var(--accent2)}"
    "label{font-size:.75em;color:var(--sub);display:block;margin-bottom:6px;margin-top:12px}"
    "input[type=range]{width:100%;accent-color:var(--accent);margin-bottom:2px}"
    "input[type=color]{width:100%;height:42px;border:none;border-radius:10px;cursor:pointer;background:none}"
    ".led-indicator{display:flex;align-items:center;gap:8px;font-size:.8em;color:var(--sub);margin-bottom:10px}"
    ".led-dot{width:8px;height:8px;border-radius:50%}"
    ".led-dot.on{background:var(--on);box-shadow:0 0 6px var(--on)}"
    ".led-dot.off{background:var(--sub)}"
    "#toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%) translateY(80px);"
    "  background:var(--card2);color:var(--text);padding:10px 20px;border-radius:20px;"
    "  font-size:.8em;transition:transform .3s;box-shadow:0 4px 20px rgba(0,0,0,.5);"
    "  border:1px solid rgba(255,255,255,.1);pointer-events:none}"
    "#toast.show{transform:translateX(-50%) translateY(0)}"
    "@keyframes pulse{0%,100%{opacity:1}50%{opacity:.4}}"
    ".pir-dot.active{animation:pulse 1.2s infinite}"
    "</style></head><body>"
    "<h1>&#9964; ESP-HOME</h1>"

    // ── Sensor Local ──
    "<div class='card'>"
    "<div class='card-title'>Sensor Local &nbsp;<span id='chipLocal' class='chip off'>LIGHT</span></div>"
    "<div class='sensor-row'>"
    "<div class='sensor-icon' id='iconLocal'>&#9728;</div>"
    "<div style='text-align:right'>"
    "<div class='sensor-state' id='stateLocal'>HAY LUZ</div>"
    "<div style='font-size:.75em;color:var(--sub);margin-top:4px'>LM393 · GPIO15</div>"
    "</div></div>"
    "</div>"

    // ── Nodo ESP32 ──
    "<div class='card'>"
    "<div class='card-title'>Nodo ESP32 &nbsp;<span id='chipNode' class='chip on'>ON</span></div>"

    // Banner visible solo cuando nodo está OFF
    "<div class='node-disabled-banner' id='nodeBanner' style='display:none'>"
    "&#9888; Nodo deshabilitado — sensores pausados"
    "</div>"

    // Contenido sensores (se oculta cuando nodo OFF)
    "<div id='nodeContent'>"
    "<div class='sensor-row' style='margin-bottom:8px'>"
    "<div style='display:flex;align-items:center;gap:10px'>"
    "<div class='sensor-icon' id='iconNode'>&#9728;</div>"
    "<div>"
    "<div style='font-size:.7em;color:var(--sub);letter-spacing:1px'>LUZ</div>"
    "<div class='sensor-state' id='stateNode'>HAY LUZ</div>"
    "</div></div>"
    "<div style='font-size:.75em;color:var(--sub)'>LM393 · ESP-NOW</div>"
    "</div>"
    "<div class='divider'></div>"
    "<div class='pir-pill inactive' id='pirPill'>"
    "<div class='pir-dot inactive' id='pirDot'></div>"
    "<div style='flex:1'>"
    "<div class='pir-label inactive' id='pirLabel'>Sin movimiento</div>"
    "<div class='pir-sub' id='pirSub'>--</div>"
    "</div>"
    "<span class='chip off' id='chipPir'>OFF</span>"
    "</div>"
    "<div class='hold-bar-wrap' id='holdWrap' style='display:none'>"
    "<div class='hold-bar' id='holdBar' style='width:100%'></div>"
    "</div>"
    "<div class='led-indicator'>"
    "<div class='led-dot off' id='ledDot'></div>"
    "<span id='ledPirLabel'>LED apagado</span>"
    "</div>"
    "<div class='divider'></div>"
    "</div>"

    // Controles siempre visibles
    "<button class='toggle active' id='btnNode' onclick='toggleNode()'>"
    "Nodo habilitado &nbsp;<span id='nodeLabel'>ON</span>"
    "</button>"
    "<button class='toggle inactive' id='btnAuto' onclick='toggleAuto()'>"
    "Modo Automatico &nbsp;<span id='autoLabel'>OFF</span>"
    "</button>"
    "</div>"

    // ── Tira LED ──
    "<div class='card'>"
    "<div class='card-title'>Tira WS2812B</div>"
    "<div class='row'>"
    "<button class='btn btn-on'  onclick='setState(\"ON\")'>&#9679; ON</button>"
    "<button class='btn btn-off' onclick='setState(\"OFF\")'>&#9675; OFF</button>"
    "</div>"
    "<label>Color</label>"
    "<input type='color' id='colorPick' value='#ff6400' oninput='sendColor()'>"
    "<label>Brillo &nbsp;<span id='briVal'>128</span></label>"
    "<input type='range' min='1' max='255' value='128' id='briRange'"
    "  oninput='document.getElementById(\"briVal\").innerText=this.value'"
    "  onchange='sendBrightness()'>"
    "<div class='divider'></div>"
    "<label>Efectos</label>"
    "<div class='row'>"
    "<button class='btn btn-fx active' id='fx-none'    onclick='setEffect(\"none\")'>Estatico</button>"
    "<button class='btn btn-fx' id='fx-blink'   onclick='setEffect(\"blink\")'>Blink</button>"
    "<button class='btn btn-fx' id='fx-fade'    onclick='setEffect(\"fade\")'>Fade</button>"
    "<button class='btn btn-fx' id='fx-rainbow' onclick='setEffect(\"rainbow\")'>Rainbow</button>"
    "</div>"
    "</div>"

    // ── LED GPIO ──
    "<div class='card'>"
    "<div class='card-title'>LED Simple GPIO2</div>"
    "<div class='row'>"
    "<button class='btn btn-on' onclick='fetch(\"/toggle\").then(()=>loadGpio())'>Toggle</button>"
    "</div>"
    "<div style='text-align:center;font-size:.85em;color:var(--sub)' id='gpioStatus'>...</div>"
    "</div>"

    "<div id='toast'></div>"
    "<script>"
    // nodeEnabled controla si se hacen fetches de sensores
    "var nodeEnabled=true;"
    "var pirHoldTotal=15000;"
    "var pirTimer=null;"
    "var nodeTimer=null;"

    "function toast(m,d=2000){"
    "  var t=document.getElementById('toast');"
    "  t.innerText=m;t.classList.add('show');"
    "  setTimeout(()=>t.classList.remove('show'),d);}"

    "function post(u,b){return fetch(u,{method:'POST',"
    "  headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});}"

    "function hexToRgb(h){return{r:parseInt(h.slice(1,3),16),"
    "  g:parseInt(h.slice(3,5),16),b:parseInt(h.slice(5,7),16)};}"

    "function fmtMs(ms){"
    "  var s=Math.ceil(ms/1000);"
    "  return s>0?s+'s restantes':'--';}"

    // Parar/arrancar timers de sensores del nodo
    "function startNodeTimers(){"
    "  if(!pirTimer)pirTimer=setInterval(loadPir,500);"
    "  if(!nodeTimer)nodeTimer=setInterval(loadNodeSensors,2000);"
    "}"
    "function stopNodeTimers(){"
    "  if(pirTimer){clearInterval(pirTimer);pirTimer=null;}"
    "  if(nodeTimer){clearInterval(nodeTimer);nodeTimer=null;}"
    "}"

    // Mostrar/ocultar contenido del nodo
    "function setNodeUI(enabled){"
    "  document.getElementById('nodeContent').style.display=enabled?'block':'none';"
    "  document.getElementById('nodeBanner').style.display=enabled?'none':'block';"
    "  var chip=document.getElementById('chipNode');"
    "  chip.innerText=enabled?'ON':'OFF';"
    "  chip.className='chip '+(enabled?'on':'disabled');"
    "  var btn=document.getElementById('btnNode');"
    "  var nl=document.getElementById('nodeLabel');"
    "  if(enabled){btn.className='toggle active';nl.innerText='ON';}"
    "  else{btn.className='toggle inactive';nl.innerText='OFF';}"
    "}"

    // Sensor local — siempre activo
    "function loadLux(){"
    "  fetch('/lux/status').then(r=>r.json()).then(d=>{"
    "    var dark=d.dark;"
    "    document.getElementById('iconLocal').innerHTML=dark?'&#9790;':'&#9728;';"
    "    var st=document.getElementById('stateLocal');"
    "    st.innerText=dark?'OSCURO':'HAY LUZ';"
    "    st.className='sensor-state '+(dark?'dark':'light');"
    "    var chip=document.getElementById('chipLocal');"
    "    chip.innerText=dark?'DARK':'LIGHT';"
    "    chip.className='chip '+(dark?'on':'off');"
    "    var ba=document.getElementById('btnAuto');"
    "    var al=document.getElementById('autoLabel');"
    "    if(d.auto){ba.className='toggle active';al.innerText='ON';}"
    "    else{ba.className='toggle inactive';al.innerText='OFF';}"
    "  });}"

    // Sensores del nodo (luz) — solo si enabled
    "function loadNodeSensors(){"
    "  if(!nodeEnabled)return;"
    "  fetch('/node/status').then(r=>r.json()).then(d=>{"
    "    if(!d.enabled)return;"
    "    var dark=d.dark;"
    "    document.getElementById('iconNode').innerHTML=dark?'&#9790;':'&#9728;';"
    "    var st=document.getElementById('stateNode');"
    "    st.innerText=dark?'OSCURO':'HAY LUZ';"
    "    st.className='sensor-state '+(dark?'dark':'light');"
    "  });}"

    // PIR — solo si enabled, para si recibe 204
    "function loadPir(){"
    "  if(!nodeEnabled)return;"
    "  fetch('/pir/status').then(r=>{"
    "    if(r.status===204){stopNodeTimers();return;}"
    "    return r.json();"
    "  }).then(d=>{"
    "    if(!d)return;"
    "    pirHoldTotal=d.hold_total_ms;"
    "    var motion=d.motion;"
    "    var hold=d.keep_on&&!motion;"
    "    var rem=d.hold_remaining_ms;"
    "    var pill=document.getElementById('pirPill');"
    "    var dot=document.getElementById('pirDot');"
    "    var lbl=document.getElementById('pirLabel');"
    "    var sub=document.getElementById('pirSub');"
    "    var chipPir=document.getElementById('chipPir');"
    "    var wrap=document.getElementById('holdWrap');"
    "    var bar=document.getElementById('holdBar');"
    "    var ledDot=document.getElementById('ledDot');"
    "    var ledLbl=document.getElementById('ledPirLabel');"
    "    if(motion){"
    "      pill.className='pir-pill active';"
    "      dot.className='pir-dot active';"
    "      lbl.className='pir-label active';"
    "      lbl.innerText='Movimiento detectado';"
    "      sub.innerText='LED encendido';"
    "      chipPir.className='chip motion';chipPir.innerText='ACTIVO';"
    "      wrap.style.display='none';"
    "    }else if(hold){"
    "      pill.className='pir-pill hold';"
    "      dot.className='pir-dot hold';"
    "      lbl.className='pir-label hold';"
    "      lbl.innerText='Retencion activa';"
    "      sub.innerText=fmtMs(rem);"
    "      chipPir.className='chip hold';chipPir.innerText='HOLD';"
    "      wrap.style.display='block';"
    "      var pct=pirHoldTotal>0?(rem/pirHoldTotal*100):0;"
    "      bar.style.width=pct+'%';"
    "    }else{"
    "      pill.className='pir-pill inactive';"
    "      dot.className='pir-dot inactive';"
    "      lbl.className='pir-label inactive';"
    "      lbl.innerText='Sin movimiento';"
    "      sub.innerText='--';"
    "      chipPir.className='chip off';chipPir.innerText='OFF';"
    "      wrap.style.display='none';"
    "    }"
    "    if(d.led_on){"
    "      ledDot.className='led-dot on';"
    "      ledLbl.innerText=motion?'LED ON — movimiento':hold?'LED ON — retencion':'LED ON';"
    "    }else{"
    "      ledDot.className='led-dot off';"
    "      ledLbl.innerText='LED apagado';"
    "    }"
    "  }).catch(()=>{});}"

    "function toggleAuto(){"
    "  fetch('/lux/auto').then(r=>r.json()).then(d=>{"
    "    var ba=document.getElementById('btnAuto');"
    "    var al=document.getElementById('autoLabel');"
    "    if(d.auto){ba.className='toggle active';al.innerText='ON';toast('Modo auto ON');}"
    "    else{ba.className='toggle inactive';al.innerText='OFF';toast('Modo auto OFF');}"
    "  });}"

    "function toggleNode(){"
    "  fetch('/node/toggle').then(r=>r.json()).then(d=>{"
    "    nodeEnabled=d.enabled;"
    "    setNodeUI(d.enabled);"
    "    if(d.enabled){"
    "      startNodeTimers();"
    "      toast('Nodo ON — sensores activos');"
    "    }else{"
    "      stopNodeTimers();"
    "      toast('Nodo OFF — sensores pausados');"
    "    }"
    "  });}"

    "function setState(s){"
    "  post('/strip/set',{state:s}).then(()=>toast('Tira: '+s));}"
    "function sendColor(){"
    "  var rgb=hexToRgb(document.getElementById('colorPick').value);"
    "  post('/strip/set',{color:rgb}).then(()=>toast('Color aplicado'));}"
    "function sendBrightness(){"
    "  var b=parseInt(document.getElementById('briRange').value);"
    "  post('/strip/set',{brightness:b}).then(()=>toast('Brillo: '+b));}"
    "function setEffect(ef){"
    "  document.querySelectorAll('.btn-fx').forEach(b=>b.classList.remove('active'));"
    "  document.getElementById('fx-'+ef).classList.add('active');"
    "  post('/strip/set',{effect:ef}).then(()=>toast('Efecto: '+ef));}"
    "function loadGpio(){"
    "  fetch('/status').then(r=>r.json()).then(d=>{"
    "    document.getElementById('gpioStatus').innerText="
    "      'Estado: '+(d.led1?'ENCENDIDO':'APAGADO');});}"

    // Init — consultar estado real del nodo antes de arrancar timers
    "fetch('/node/status').then(r=>r.json()).then(d=>{"
    "  nodeEnabled=d.enabled;"
    "  setNodeUI(d.enabled);"
    "  var ba=document.getElementById('btnAuto');"
    "  var al=document.getElementById('autoLabel');"
    "  if(d.auto){ba.className='toggle active';al.innerText='ON';}"
    "  if(d.enabled){loadNodeSensors();loadPir();startNodeTimers();}"
    "});"
    "loadLux();loadGpio();"
    "setInterval(loadLux,3000);"
    "setInterval(loadGpio,5000);"
    "</script></body></html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void start_webserver(Controller *controller) {
    global_ctrl = controller;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 14;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Error al iniciar webserver");
        return;
    }
    httpd_uri_t uris[] = {
        { .uri="/",             .method=HTTP_GET,  .handler=root_handler         },
        { .uri="/toggle",       .method=HTTP_GET,  .handler=toggle_handler       },
        { .uri="/status",       .method=HTTP_GET,  .handler=status_handler       },
        { .uri="/strip/status", .method=HTTP_GET,  .handler=strip_status_handler },
        { .uri="/strip/set",    .method=HTTP_POST, .handler=strip_set_handler    },
        { .uri="/lux/status",   .method=HTTP_GET,  .handler=lux_status_handler   },
        { .uri="/lux/auto",     .method=HTTP_GET,  .handler=lux_auto_handler     },
        { .uri="/node/status",  .method=HTTP_GET,  .handler=node_status_handler  },
        { .uri="/node/toggle",  .method=HTTP_GET,  .handler=node_toggle_handler  },
        { .uri="/pir/status",   .method=HTTP_GET,  .handler=pir_status_handler   },
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
