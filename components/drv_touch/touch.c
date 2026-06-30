#include "touch.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "touch";

#define I2C_PORT     I2C_NUM_0
#define GT911_ADDR   0x14

static esp_err_t gt911_read_reg(uint16_t reg, uint8_t *data, size_t len)
{
    // 事务1：写 2 字节寄存器地址 + STOP
    uint8_t rb[2] = {reg >> 8, reg & 0xFF};
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (GT911_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, rb[0], true);
    i2c_master_write_byte(cmd, rb[1], true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(200));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) return ret;

    // 事务2：读 len 字节
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (GT911_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(200));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t gt911_write_reg(uint16_t reg, uint8_t val)
{
    uint8_t buf[3] = {reg >> 8, reg & 0xFF, val};
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (GT911_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, buf[0], true);
    i2c_master_write_byte(cmd, buf[1], true);
    i2c_master_write_byte(cmd, buf[2], true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(200));
    i2c_cmd_link_delete(cmd);
    return ret;
}

bool touch_init(void)
{
    // 简单地址确认
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (GT911_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(200));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "GT911 初始化成功 (0x14)");
        gt911_write_reg(0x814E, 0);
        return true;
    }
    ESP_LOGW(TAG, "GT911 不在线，触控将在 PCB 阶段修复");
    return false;
}

bool touch_read(uint16_t *x, uint16_t *y)
{
    uint8_t status;
    if (gt911_read_reg(0x814E, &status, 1) != ESP_OK) return false;
    if (!(status & 0x80)) return false;

    uint8_t pts = status & 0x0F;
    if (pts == 0) return false;

    uint8_t coord[5];
    if (gt911_read_reg(0x814F, coord, 5) != ESP_OK) return false;

    *x = (coord[1] << 8) | coord[0];
    *y = (coord[3] << 8) | coord[2];
    gt911_write_reg(0x814E, 0);
    return true;
}
