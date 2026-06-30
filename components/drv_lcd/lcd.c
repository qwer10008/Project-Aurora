#include "lcd.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "lcd";

#define LCD_H_RES        800
#define LCD_V_RES        480
#define LCD_PCLK_HZ      (20 * 1000 * 1000)
#define LCD_HSYNC_PW     48
#define LCD_HSYNC_BP     88
#define LCD_HSYNC_FP     40
#define LCD_VSYNC_PW     3
#define LCD_VSYNC_BP     32
#define LCD_VSYNC_FP     13

esp_lcd_panel_handle_t lcd_init(void)
{
    ESP_LOGI(TAG, "初始化背光...");
    gpio_set_direction(38, GPIO_MODE_OUTPUT);
    gpio_set_level(38, 1);

    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .data_width = 16,
        .psram_trans_align = 64,
        .num_fbs = 1,
        .bounce_buffer_size_px = 4800,
        .timings = {
            .pclk_hz = LCD_PCLK_HZ,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_pulse_width = LCD_HSYNC_PW,
            .hsync_back_porch = LCD_HSYNC_BP,
            .hsync_front_porch = LCD_HSYNC_FP,
            .vsync_pulse_width = LCD_VSYNC_PW,
            .vsync_back_porch = LCD_VSYNC_BP,
            .vsync_front_porch = LCD_VSYNC_FP,
            .flags.pclk_active_neg = 1,
        },
        .hsync_gpio_num = 8,
        .vsync_gpio_num = 6,
        .de_gpio_num = 15,
        .pclk_gpio_num = 14,
        .disp_gpio_num = -1,
        .data_gpio_nums = {
            9, 10, 11, 12, 13, 3, 46, 16,
            17, 18, 40, 21, 47, 48, 45, 39,
        },
        .flags.fb_in_psram = 1,
    };

    esp_lcd_panel_handle_t panel = NULL;
    esp_err_t err = esp_lcd_new_rgb_panel(&cfg, &panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RGB 面板创建失败: %s", esp_err_to_name(err));
        return NULL;
    }

    err = esp_lcd_panel_reset(panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "复位失败: %s", esp_err_to_name(err));
    }

    // 先获取帧缓冲、填好数据，再初始化面板
    void *fb = NULL;
    err = esp_lcd_rgb_panel_get_frame_buffer(panel, 1, &fb);
    if (err != ESP_OK || fb == NULL) {
        ESP_LOGE(TAG, "帧缓冲失败: %s", esp_err_to_name(err));
        return NULL;
    }

    uint16_t *buf = (uint16_t *)fb;
    for (int y = 0; y < LCD_V_RES; y++) {
        for (int x = 0; x < LCD_H_RES; x++) {
            buf[y * LCD_H_RES + x] = 0xFFFF;  // 白屏
        }
    }

    err = esp_lcd_panel_init(panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "面板初始化失败: %s", esp_err_to_name(err));
        return NULL;
    }
    ESP_LOGI(TAG, "LCD 初始化完成");
    return panel;
}

void *lcd_get_fb(esp_lcd_panel_handle_t panel)
{
    void *fb = NULL;
    esp_err_t err = esp_lcd_rgb_panel_get_frame_buffer(panel, 1, &fb);
    if (err != ESP_OK || fb == NULL) {
        ESP_LOGE(TAG, "获取帧缓冲失败");
    }
    return fb;
}
