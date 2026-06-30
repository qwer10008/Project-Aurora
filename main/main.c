/*
 * 完全复用 GitHub 屏幕测试代码 (commit 72ba989)
 * 彩条测试：红/绿/蓝三色，验证 LCD 基本功能
 */
#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"

static const char *TAG = "lcd_test";

#define LCD_H_RES        800
#define LCD_V_RES        480
#define LCD_PCLK_HZ      (20 * 1000 * 1000)
#define LCD_HSYNC_PW     48
#define LCD_HSYNC_BP     88
#define LCD_HSYNC_FP     40
#define LCD_VSYNC_PW     3
#define LCD_VSYNC_BP     32
#define LCD_VSYNC_FP     13

void app_main(void)
{
    ESP_LOGI(TAG, "初始化 RGB LCD...");

    // 背光拉高
    gpio_set_direction(38, GPIO_MODE_OUTPUT);
    gpio_set_level(38, 1);

    // 配置 RGB 面板
    esp_lcd_rgb_panel_config_t rgb_config = {
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

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_err_t err = esp_lcd_new_rgb_panel(&rgb_config, &panel_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RGB 面板创建失败: %s", esp_err_to_name(err));
        return;
    }

    err = esp_lcd_panel_reset(panel_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "复位失败: %s", esp_err_to_name(err));
    }

    // ★ 先获取帧缓冲、填好数据，再初始化面板 ★
    void *fb = NULL;
    err = esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 1, &fb);
    if (err != ESP_OK || fb == NULL) {
        ESP_LOGE(TAG, "帧缓冲失败: %s", esp_err_to_name(err));
        return;
    }

    uint16_t *buf = (uint16_t *)fb;
    int stripe_w = LCD_H_RES / 3;

    for (int y = 0; y < LCD_V_RES; y++) {
        for (int x = 0; x < LCD_H_RES; x++) {
            uint16_t color;
            if (x < stripe_w) {
                color = 0xF800;   // 红色
            } else if (x < stripe_w * 2) {
                color = 0x07E0;   // 绿色
            } else {
                color = 0x001F;   // 蓝色
            }
            buf[y * LCD_H_RES + x] = color;
        }
    }

    ESP_LOGI(TAG, "彩条已写入，启动面板");

    // 缓冲填好了，现在启动 RGB 输出
    err = esp_lcd_panel_init(panel_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "面板初始化失败: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "面板正在输出");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        printf(".");
        fflush(stdout);
    }
}
