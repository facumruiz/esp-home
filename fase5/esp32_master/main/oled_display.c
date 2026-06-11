#include "oled_display.h"
#include "config.h"
#include "controller.h"
#include "espnow_master.h"
#include "qrcodegen.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "u8g2_esp32_hal.h"

static const char *TAG = "OLED";
static u8g2_t u8g2;
static uint8_t s_screen = 0; // 0=QR, 1=estado

void oled_init(void) {
    u8g2_esp32_hal_t hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal_init(hal);

    u8g2_Setup_sh1106_i2c_128x64_noname_f(
        &u8g2, U8G2_R2,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb
    );
    u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_SetContrast(&u8g2, 255);
    u8g2_ClearDisplay(&u8g2);
    ESP_LOGI(TAG, "OLED SH1106 init OK");
}

static void draw_qr(void) {
    // Contenido WiFi para conectar al AP
    const char *wifi_str = "http://192.168.4.1";

    static uint8_t qr_data[qrcodegen_BUFFER_LEN_MAX];
    static uint8_t tmp[qrcodegen_BUFFER_LEN_MAX];

    bool ok = qrcodegen_encodeText(
        wifi_str, tmp, qr_data,
        qrcodegen_Ecc_LOW,
        qrcodegen_VERSION_MIN,
        qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO,
        true
    );

    if (!ok) {
        u8g2_SetFont(&u8g2, u8g2_font_5x7_tf);
        u8g2_DrawStr(&u8g2, 0, 32, "QR error");
        return;
    }

    int size = qrcodegen_getSize(qr_data);

    // Escalar para que entre en 64x64 (lado izquierdo)
    int scale = 2;
    
    int offset_x = (64 - size * scale) / 2;
    int offset_y = (64 - size * scale) / 2;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (qrcodegen_getModule(qr_data, x, y)) {
                u8g2_DrawBox(&u8g2,
                    offset_x + x * scale,
                    offset_y + y * scale,
                    scale, scale);
            }
        }
    }

    // Texto al costado derecho
    u8g2_SetFont(&u8g2, u8g2_font_4x6_tf);
    u8g2_DrawStr(&u8g2, 68, 10, "Conectar");
    u8g2_DrawStr(&u8g2, 68, 20, "WiFi:");
    u8g2_DrawStr(&u8g2, 68, 30, "Domotica");
    u8g2_DrawStr(&u8g2, 68, 40, "Local");
    u8g2_DrawStr(&u8g2, 68, 52, "192.168.");
    u8g2_DrawStr(&u8g2, 68, 62, "4.1");
}

static void draw_status(Controller *ctrl) {
    bool online = espnow_node_is_online();
    bool motion = online && g_node_status.motion;
    bool dark   = online && g_node_status.dark;

    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&u8g2, 0, 10, "ESP-HOME");
    if (online)
        u8g2_DrawDisc(&u8g2, 122, 6, 4, U8G2_DRAW_ALL);
    else
        u8g2_DrawCircle(&u8g2, 122, 6, 4, U8G2_DRAW_ALL);
    u8g2_DrawHLine(&u8g2, 0, 13, 128);

    u8g2_SetFont(&u8g2, u8g2_font_5x7_tf);
    char buf[24];

    snprintf(buf, sizeof(buf), "T1: %s", ctrl->strip1->state ? "ON" : "OFF");
    u8g2_DrawStr(&u8g2, 0, 25, buf);

    snprintf(buf, sizeof(buf), "T2: %s", ctrl->strip2->state ? "ON" : "OFF");
    u8g2_DrawStr(&u8g2, 64, 25, buf);

    snprintf(buf, sizeof(buf), "PIR:%s", motion ? "MOVIM" : "QUIET");
    u8g2_DrawStr(&u8g2, 0, 36, buf);

    snprintf(buf, sizeof(buf), "LUZ:%s", !online ? "--" : dark ? "OSCUR" : "LUZ");
    u8g2_DrawStr(&u8g2, 64, 36, buf);

    snprintf(buf, sizeof(buf), "AUTO:%s", ctrl->auto_mode ? "ON" : "OFF");
    u8g2_DrawStr(&u8g2, 0, 47, buf);

    snprintf(buf, sizeof(buf), "NODO:%s", online ? "ON" : "OFF");
    u8g2_DrawStr(&u8g2, 64, 47, buf);

    u8g2_DrawStr(&u8g2, 0, 58, "192.168.4.1");
}

void oled_task(void *arg) {
    Controller *ctrl = (Controller *)arg;
    oled_init();

    uint32_t tick = 0;
    while (1) {
        u8g2_ClearBuffer(&u8g2);

        if (s_screen == 0) {
            draw_qr();
        } else {
            draw_status(ctrl);
        }

        u8g2_SendBuffer(&u8g2);

        // Rotar pantalla cada 10 segundos
        tick++;
        if (tick >= 10) {
            tick = 0;
            s_screen = (s_screen + 1) % 2;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
