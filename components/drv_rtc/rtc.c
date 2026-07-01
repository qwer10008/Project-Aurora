#include "ds3231.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "rtc";

#define I2C_PORT     I2C_NUM_0
#define I2C_SDA      41
#define I2C_SCL      42
#define I2C_FREQ     10000
#define DS3231_ADDR  0x68

static bool i2c_installed = false;

/*****************************************************************************
 * DS3231 寄存器映射（数据手册 Rev.10, Page 12, Table 2）
 *   0x00: 秒     (bit 7 = 振荡器停振标志 OSF)
 *   0x01: 分
 *   0x02: 时     (bit 6 = 12/24 小时模式选择: 0=24h, 1=12h)
 *   0x03: 星期
 *   0x04: 日
 *   0x05: 月/世纪
 *   0x06: 年
 *   0x0E: 控制寄存器
 *   0x0F: 状态寄存器
 *****************************************************************************/
#define REG_SEC           0x00
#define REG_HOUR          0x02
#define REG_CTRL          0x0E
#define REG_STATUS        0x0F

/* I2C 辅助函数：写多字节 */
static esp_err_t i2c_write_bytes(uint8_t reg, const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    for (size_t i = 0; i < len; i++) {
        i2c_master_write_byte(cmd, data[i], true);
    }
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/* I2C 辅助函数：写 1 字节 */
static esp_err_t i2c_write_reg(uint8_t reg, uint8_t val)
{
    return i2c_write_bytes(reg, &val, 1);
}

/* I2C 辅助函数：读多字节 */
static esp_err_t i2c_read_bytes(uint8_t reg, uint8_t *buf, size_t len)
{
    // 写寄存器地址
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) return ret;

    // 读数据
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, buf, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/* BCD 校验：检查 bcd 是否是不超过 max 的有效 BCD 值 */
static bool bcd_valid(uint8_t bcd, uint8_t max_dec)
{
    uint8_t tens = max_dec / 10;
    return ((bcd & 0x0F) <= 0x09) && ((bcd >> 4) <= tens);
}

/* BCD → 十进制 */
static uint8_t bcd2dec(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

/* 十进制 → BCD */
static uint8_t dec2bcd(uint8_t dec)
{
    return ((dec / 10) << 4) | (dec % 10);
}

void ds3231_init(void)
{
    if (i2c_installed) return;

    ESP_LOGI(TAG, "安装 I2C 总线 (GPIO41/42, 10kHz)...");
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

    /* 等待 DS3231 内部振荡器稳定（数据手册建议上电后等待 ~1ms） */
    vTaskDelay(pdMS_TO_TICKS(2));

    /*
     * 强制设为 24 小时模式：
     *   读小时寄存器，如果 bit 6 = 1（12h 模式），清除它。
     *   数据手册 Rev.10, Page 14: 小时寄存器 bit 6 = 12/24 模式选择
     */
    uint8_t hour_val;
    if (i2c_read_bytes(REG_HOUR, &hour_val, 1) == ESP_OK) {
        if (hour_val & 0x40) {
            ESP_LOGI(TAG, "DS3231 处于 12h 模式 (0x%02X)，强制切换为 24h", hour_val);
            hour_val &= ~0x40;              // 清除 12h 标志
            hour_val &= ~0x60;              // 清除 AM/PM bit (bit 5)
            i2c_write_reg(REG_HOUR, hour_val);
        }
    }

    /*
     * 初始化控制寄存器（0x0E）：
     *   默认出厂值 = 0x00。
     *   写入 0x00 确保：
     *     - 不使用 INTCN（bit 2 = 0）
     *     - 不使用 SQW 输出（bits 4:3 = 00）
     *     - 不使用 32kHz 输出（bit 7 = 0）
     *   数据手册 Rev.10, Page 15
     */
    i2c_write_reg(REG_CTRL, 0x00);

    /*
     * 读取并清除状态寄存器的 OSF（Oscillator Stop Flag）：
     *   如果芯片第一次上电或电池耗尽，OSF = 1，时间寄存器包含随机值。
     *   清除 OSF：写 0x00 到 0x0F。
     *   数据手册 Rev.10, Page 16
     */
    uint8_t status;
    if (i2c_read_bytes(REG_STATUS, &status, 1) == ESP_OK) {
        ESP_LOGI(TAG, "DS3231 状态寄存器: 0x%02X", status);
        if (status & 0x80) {
            ESP_LOGW(TAG, "OSF 置位！芯片曾经停振，时间无效，正在清除...");
            i2c_write_reg(REG_STATUS, 0x00);
        }
    }

    ESP_LOGI(TAG, "DS3231 初始化完成");
}

bool ds3231_read_time(rtc_time_t *t)
{
    uint8_t d[7];
    if (i2c_read_bytes(REG_SEC, d, 7) != ESP_OK) {
        return false;
    }

    /*
     * 校验每个寄存器是否包含有效 BCD 值。
     * 小时 (d[2])：bit 6 已被 ds3231_init 清除为 24h 模式，
     * 但如果用户从未调用 init（或 DS3231 被外部复位），bit 6 可能仍为 1。
     * 这里再做一次安全处理。
     */
    d[2] &= ~0x40;   /* 清除 12h 标志（bit 6） */
    d[2] &= ~0x20;   /* 清除 AM/PM 位（bit 5） */

    /* 校验范围 */
    if (!bcd_valid(d[0], 59)) return false;  /* 秒 */
    if (!bcd_valid(d[1], 59)) return false;  /* 分 */
    if (!bcd_valid(d[2], 23)) return false;  /* 时（24h 模式） */
    if (!bcd_valid(d[4], 31)) return false;  /* 日 */
    if (!bcd_valid(d[5], 12)) return false;  /* 月 */
    /* 年 (d[6])：BCD 0x00-0x99，不需要校验上限，全范围有效 */

    t->sec   = bcd2dec(d[0]);
    t->min   = bcd2dec(d[1]);
    t->hour  = bcd2dec(d[2]);
    t->day   = bcd2dec(d[4]);
    t->month = bcd2dec(d[5]);
    t->year  = bcd2dec(d[6]);

    /* 最终合理性检查：如果年月日全为 0，大概率是垃圾数据 */
    if (t->year == 0 && t->month == 0 && t->day == 0) {
        return false;
    }

    return true;
}

void ds3231_write_time(const rtc_time_t *t)
{
    uint8_t data[8] = {
        REG_SEC,                      // 起始寄存器地址
        dec2bcd(t->sec),              // 秒
        dec2bcd(t->min),              // 分
        dec2bcd(t->hour),             // 时（24h 模式，bit 6 = 0）
        1,                            // 星期（随便给 1=周一）
        dec2bcd(t->day),             // 日
        dec2bcd(t->month),            // 月
        dec2bcd(t->year),             // 年
    };
    i2c_write_bytes(REG_SEC, data + 1, 7);
}
