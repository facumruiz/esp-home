#include "led_strip_ws.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "LED_STRIP_WS";

static inline uint8_t apply_brightness(uint8_t channel, uint8_t brightness) {
    return (uint8_t)((uint32_t)channel * brightness / 255);
}

static void write_color(LedStrip *strip, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t br = apply_brightness(r, strip->brightness);
    uint8_t bg = apply_brightness(g, strip->brightness);
    uint8_t bb = apply_brightness(b, strip->brightness);
    for (uint32_t i = 0; i < strip->num_leds; i++) {
        led_strip_set_pixel(strip->handle, i, br, bg, bb);
    }
    led_strip_refresh(strip->handle);
}

static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v,
                        uint8_t *r, uint8_t *g, uint8_t *b) {
    if (s == 0) { *r = *g = *b = v; return; }
    uint8_t region = h / 60;
    uint8_t rem    = (h % 60) * 255 / 60;
    uint8_t p = (uint32_t)v * (255 - s) / 255;
    uint8_t q = (uint32_t)v * (255 - ((uint32_t)s * rem / 255)) / 255;
    uint8_t t = (uint32_t)v * (255 - ((uint32_t)s * (255 - rem) / 255)) / 255;
    switch (region) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

esp_err_t led_strip_ws_init(LedStrip *strip, gpio_num_t gpio, uint32_t num_leds) {
    strip->num_leds   = num_leds;
    strip->state      = false;
    strip->r = strip->g = strip->b = 255;
    strip->brightness = 128;
    strip->effect     = STRIP_EFFECT_NONE;

    led_strip_config_t cfg = {
        .strip_gpio_num         = gpio,
        .max_leds               = num_leds,
        .led_model              = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out       = false,
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src        = RMT_CLK_SRC_DEFAULT,
        .resolution_hz  = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    esp_err_t err = led_strip_new_rmt_device(&cfg, &rmt_cfg, &strip->handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error init RMT: %s", esp_err_to_name(err));
        return err;
    }
    led_strip_clear(strip->handle);
    ESP_LOGI(TAG, "WS2812 init OK — GPIO %d, %lu LEDs", gpio, (unsigned long)num_leds);
    return ESP_OK;
}

void led_strip_ws_set_state(LedStrip *strip, bool on)              { strip->state      = on;         }
void led_strip_ws_set_color(LedStrip *strip, uint8_t r, uint8_t g, uint8_t b) { strip->r = r; strip->g = g; strip->b = b; }
void led_strip_ws_set_brightness(LedStrip *strip, uint8_t brightness) { strip->brightness = brightness; }
void led_strip_ws_set_effect(LedStrip *strip, strip_effect_t effect)  { strip->effect     = effect;     }
void led_strip_ws_clear(LedStrip *strip)                           { led_strip_clear(strip->handle);  }

void led_strip_ws_apply(LedStrip *strip) {
    if (!strip->state) { led_strip_clear(strip->handle); return; }
    write_color(strip, strip->r, strip->g, strip->b);
}

void led_strip_ws_task(void *arg) {
    LedStrip *strip   = (LedStrip *)arg;
    uint16_t  hue     = 0;
    uint8_t   fade_val = 0;
    bool      fade_up  = true;
    bool      blink_on = false;

    while (1) {
        if (!strip->state || strip->effect == STRIP_EFFECT_NONE) {
            led_strip_ws_apply(strip);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        switch (strip->effect) {
            case STRIP_EFFECT_BLINK:
                blink_on = !blink_on;
                if (blink_on) write_color(strip, strip->r, strip->g, strip->b);
                else          led_strip_clear(strip->handle);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;

            case STRIP_EFFECT_FADE: {
                uint8_t br = apply_brightness(strip->r, fade_val);
                uint8_t bg = apply_brightness(strip->g, fade_val);
                uint8_t bb = apply_brightness(strip->b, fade_val);
                write_color(strip, br, bg, bb);
                if (fade_up) { fade_val += 5; if (fade_val >= strip->brightness) fade_up = false; }
                else         { if (fade_val >= 5) fade_val -= 5; else fade_up = true; }
                vTaskDelay(pdMS_TO_TICKS(30));
                break;
            }

            case STRIP_EFFECT_RAINBOW: {
                uint8_t r, g, b;
                hsv_to_rgb(hue, 255, strip->brightness, &r, &g, &b);
                write_color(strip, r, g, b);
                hue = (hue + 3) % 360;
                vTaskDelay(pdMS_TO_TICKS(20));
                break;
            }

            default:
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
    }
}
