#include "u8g2_esp32_hal.h"
#include "driver/i2c.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define I2C_PORT    I2C_NUM_0
#define I2C_TIMEOUT pdMS_TO_TICKS(100)

static const char *TAG = "U8G2_HAL";
static i2c_cmd_handle_t s_cmd = NULL;
static uint8_t s_i2c_addr = 0x78;

void u8g2_esp32_hal_init(u8g2_esp32_hal_t hal) {
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = hal.sda,
        .scl_io_num       = hal.scl,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    ESP_LOGI(TAG, "I2C init OK");
}

uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
    case U8X8_MSG_BYTE_INIT:
        s_i2c_addr = u8x8_GetI2CAddress(u8x8);
        break;

    case U8X8_MSG_BYTE_SEND:
        i2c_master_write(s_cmd, (uint8_t *)arg_ptr, arg_int, true);
        break;

    case U8X8_MSG_BYTE_START_TRANSFER:
        s_cmd = i2c_cmd_link_create();
        i2c_master_start(s_cmd);
        i2c_master_write_byte(s_cmd, s_i2c_addr | I2C_MASTER_WRITE, true);
        break;

    case U8X8_MSG_BYTE_END_TRANSFER:
        i2c_master_stop(s_cmd);
        i2c_master_cmd_begin(I2C_PORT, s_cmd, I2C_TIMEOUT);
        i2c_cmd_link_delete(s_cmd);
        s_cmd = NULL;
        break;

    case U8X8_MSG_BYTE_SET_DC:
        // No aplica para I2C
        break;

    default:
        break;
    }
    return 1;
}

uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg,
                                      uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        break;
    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(pdMS_TO_TICKS(arg_int ? arg_int : 1));
        break;
    case U8X8_MSG_DELAY_10MICRO:
        esp_rom_delay_us(10 * arg_int);
        break;
    case U8X8_MSG_DELAY_100NANO:
        esp_rom_delay_us(1);
        break;
    case U8X8_MSG_GPIO_RESET:
        break;
    default:
        break;
    }
    return 1;
}
