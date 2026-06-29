#include "driver/i2c.h"

#define I2C_MASTER_SCL_IO    42
#define I2C_MASTER_SDA_IO    41
#define I2C_MASTER_FREQ_HZ   50000
#define I2C_MASTER_PORT      I2C_NUM_0
#define DS3231_ADDR           0x68

static uint8_t bcd2dec(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static esp_err_t ds3231_read_time(uint8_t *data)
{
    uint8_t reg = 0x00;

    // 第1步：写寄存器地址（单独事务，结束于 STOP）
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) return ret;

    // 第2步：读 7 字节（单独事务）
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 7, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static void ds3231_write_time(uint8_t h, uint8_t m, uint8_t s,                               
uint8_t y, uint8_t mo, uint8_t d)
{    
uint8_t data[8];    
data[0] = 0x00;   // 起始寄存器地址    
data[1] = s;      // 秒 (BCD)    
data[2] = m;      // 分 (BCD)    
data[3] = h;      // 时 (BCD, 24h)    
data[4] = 1;      // 星期（随便填）    
data[5] = d;      // 日 (BCD)    
data[6] = mo;     // 月 (BCD)    
data[7] = y;      // 年 (BCD)
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();    
i2c_master_start(cmd);    
i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);    
i2c_master_write(cmd, data, 8, true);    
i2c_master_stop(cmd);    
i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, pdMS_TO_TICKS(50));    
i2c_cmd_link_delete(cmd);
}
// 十进制 → BCD
static uint8_t dec2bcd(uint8_t dec) {    
return ((dec / 10) << 4) | (dec % 10);
}
void app_main(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_PORT, &conf);
    i2c_driver_install(I2C_MASTER_PORT, conf.mode, 0, 0, 0);
    // ★ 首次初始化：写一次当前时间（执行一次后注释掉这行）    
    ds3231_write_time(dec2bcd(17), dec2bcd(30), dec2bcd(0),   // 17:30:00                      
    dec2bcd(26), dec2bcd(6), dec2bcd(30));  // 2026-06-30
// 在 app_main 的 while(1) 中：    
        while (1) {        
        uint8_t d[7];        
if (ds3231_read_time(d) == ESP_OK) {            
uint8_t s = d[0];            
// 校验秒在合法范围            
if ((s & 0x0F) <= 0x09 && (s >> 4) <= 0x05) {                
printf("20%02X-%02X-%02X  %02X:%02X:%02X\n",                       
d[6], d[5], d[4], d[2], d[1], s);            
} else {                
printf("— 跳过非法秒: 0x%02X\n", s);            
}        
}        
vTaskDelay(pdMS_TO_TICKS(200));    
}
}