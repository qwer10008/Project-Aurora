#include "touch.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "touch";

#define I2C_PORT     I2C_NUM_0
#define TP_RST       2
#define TP_INT       1

static uint8_t gt911_addr = 0x5D;  // 运行时可变

static esp_err_t gt911_read_reg(uint16_t reg, uint8_t *data, size_t len)
{
    uint8_t reg_buf[2] = { (reg >> 8) & 0xFF, reg & 0xFF };
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (gt911_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, reg_buf, 2, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) return ret;

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (gt911_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t gt911_write_reg(uint16_t reg, uint8_t val)
{
    uint8_t buf[3] = { (reg >> 8) & 0xFF, reg & 0xFF, val };
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (gt911_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, buf, 3, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

bool touch_init(void)
{
    // 等 LCD RGB 噪声稳定后再访问 I2C
    vTaskDelay(pdMS_TO_TICKS(200));

    // 轮询 GT911
    uint8_t addrs[] = {0x14, 0x5D};
    for (int a = 0; a < 2; a++) {
        gt911_addr = addrs[a];
        for (int retry = 0; retry < 5; retry++) {
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (gt911_addr << 1) | I2C_MASTER_WRITE, true);
            i2c_master_stop(cmd);
            esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
            i2c_cmd_link_delete(cmd);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "GT911 初始化成功 (0x%02X, 重试 %d)", gt911_addr, retry);
                gt911_write_reg(0x814E, 0);
                return true;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    ESP_LOGE(TAG, "GT911 初始化失败");
    return false;
}

bool touch_read(uint16_t *x, uint16_t *y)
{
    uint8_t status;
    if (gt911_read_reg(0x814E, &status, 1) != ESP_OK) {
        static int err_cnt = 0;
        if (++err_cnt <= 3) ESP_LOGW(TAG, "读 GT911 状态失败 (第 %d 次)", err_cnt);
        return false;
    }
    if (!(status & 0x80)) return false;

    uint8_t pts = status & 0x0F;
    if (pts == 0) return false;

    uint8_t coord[5];
    if (gt911_read_reg(0x814F, coord, 5) != ESP_OK) return false;

    *x = (coord[1] << 8) | coord[0];
    *y = (coord[3] << 8) | coord[2];
    ESP_LOGI(TAG, "触摸: X=%d Y=%d", *x, *y);

    gt911_write_reg(0x814E, 0);
    return true;
}
