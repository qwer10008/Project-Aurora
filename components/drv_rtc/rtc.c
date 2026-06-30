#include "rtc.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "rtc";

#define I2C_PORT     I2C_NUM_0
#define I2C_SDA      41
#define I2C_SCL      42
#define I2C_FREQ     10000
#define DS3231_ADDR  0x68

static bool i2c_installed = false;

void ds3231_init(void)
{
    if (i2c_installed) return;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ,
    };
    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
    i2c_installed = true;
}

static bool bcd_valid(uint8_t bcd, uint8_t max)
{
    return (bcd & 0x0F) <= 0x09 && (bcd >> 4) <= (max / 10);
}

bool ds3231_read_time(rtc_time_t *t)
{
    // 第1步：写寄存器地址
    uint8_t reg = 0x00;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) return false;

    // 第2步：读7字节
    uint8_t d[7];
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, d, 7, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) return false;

    // 校验秒
    if (!bcd_valid(d[0], 59)) return false;
    t->sec   = ((d[0] >> 4) * 10) + (d[0] & 0x0F);
    t->min   = ((d[1] >> 4) * 10) + (d[1] & 0x0F);
    t->hour  = ((d[2] >> 4) * 10) + (d[2] & 0x0F);
    t->day   = ((d[4] >> 4) * 10) + (d[4] & 0x0F);
    t->month = ((d[5] >> 4) * 10) + (d[5] & 0x0F);
    t->year  = ((d[6] >> 4) * 10) + (d[6] & 0x0F);
    return true;
}

static uint8_t dec2bcd(uint8_t dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

void ds3231_write_time(const rtc_time_t *t)
{
    uint8_t data[8] = {
        0x00,
        dec2bcd(t->sec),
        dec2bcd(t->min),
        dec2bcd(t->hour),
        1,                    // 星期（随便）
        dec2bcd(t->day),
        dec2bcd(t->month),
        dec2bcd(t->year),
    };
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, 8, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
}
