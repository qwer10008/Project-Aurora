#include "driver/i2c.h"

#define I2C_MASTER_SCL_IO    42
#define I2C_MASTER_SDA_IO    41
#define I2C_MASTER_FREQ_HZ   100000
#define I2C_MASTER_PORT      I2C_NUM_0

void app_main(void)
{
    // 初始化 I2C
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

    printf("I2C 扫描开始...\n");

    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        if (ret == ESP_OK) {
            printf("发现设备 0x%02X\n", addr);
        }
    }

    printf("扫描完成\n");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}