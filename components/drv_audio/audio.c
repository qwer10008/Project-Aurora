#include "audio.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include <math.h>
#include <stdlib.h>
#include "esp_log.h"

static const char *TAG = "audio";
static i2s_chan_handle_t tx_chan = NULL;
static bool initialized = false;

static const int SAMPLE_RATE = 16000;

void audio_init(void)
{
    if (initialized) return;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = 4,
            .ws = 5,
            .dout = 7,
            .din = I2S_GPIO_UNUSED,
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
    initialized = true;
    ESP_LOGI(TAG, "音频初始化完成");
}

void audio_play_tone(int freq_hz, int duration_ms)
{
    if (!initialized) return;

    int samples = SAMPLE_RATE * duration_ms / 1000;
    int16_t *buf = malloc(samples * sizeof(int16_t));
    if (buf == NULL) return;

    for (int i = 0; i < samples; i++) {
        buf[i] = (int16_t)(28000.0 * sin(2.0 * M_PI * freq_hz * i / SAMPLE_RATE));
    }

    size_t written;
    i2s_channel_write(tx_chan, buf, samples * sizeof(int16_t), &written, portMAX_DELAY);
    free(buf);
    ESP_LOGI(TAG, "播放 %dHz %dms 完成", freq_hz, duration_ms);
}
