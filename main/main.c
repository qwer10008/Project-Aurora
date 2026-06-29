#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include <math.h>

void app_main(void)
{
    i2s_chan_handle_t tx_chan = NULL;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, &tx_chan, NULL);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = 4,
            .ws = 5,
            .dout = 7,
            .din = I2S_GPIO_UNUSED,
        },
    };
    i2s_channel_init_std_mode(tx_chan, &std_cfg);
    i2s_channel_enable(tx_chan);

    printf("播放 440Hz 测试音...\n");

    int16_t *buf = malloc(16000 * sizeof(int16_t));
    if (buf == NULL) {
        printf("内存分配失败\n");
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // ★ 这行 for 循环之前被删了，加上！★
    for (int i = 0; i < 16000; i++) {
        buf[i] = (int16_t)(28000.0 * sin(2.0 * 3.14159 * 440.0 * i / 16000.0));
    }

    size_t written;
    i2s_channel_write(tx_chan, buf, 16000 * sizeof(int16_t), &written, portMAX_DELAY);
    printf("写入 %d 字节，应听到滴声\n", (int)written);

    free(buf);  // 用完释放
    vTaskDelay(pdMS_TO_TICKS(2000));
    i2s_channel_disable(tx_chan);

    printf("测试结束\n");
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}