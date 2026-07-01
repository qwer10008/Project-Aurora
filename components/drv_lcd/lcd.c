#include "lcd.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_cache.h"

static const char *TAG = "lcd";

/* Original 20MHz timing — this worked before */
#define LCD_H_RES        800
#define LCD_V_RES        480
#define LCD_PCLK_HZ      (20 * 1000 * 1000)
#define LCD_HSYNC_PW     48
#define LCD_HSYNC_BP     88
#define LCD_HSYNC_FP     40
#define LCD_VSYNC_PW     3
#define LCD_VSYNC_BP     32
#define LCD_VSYNC_FP     13
#define LCD_BL_GPIO      38

esp_lcd_panel_handle_t lcd_init(void)
{
    int h_total = LCD_H_RES + LCD_HSYNC_PW + LCD_HSYNC_BP + LCD_HSYNC_FP;
    int v_total = LCD_V_RES + LCD_VSYNC_PW + LCD_VSYNC_BP + LCD_VSYNC_FP;
    ESP_LOGI(TAG, "Timing: %dx%d PCLK=%dM H=%d V=%d -> ~%dfps",
             LCD_H_RES, LCD_V_RES, LCD_PCLK_HZ/1000000,
             h_total, v_total, LCD_PCLK_HZ/(h_total*v_total));

    gpio_set_direction(LCD_BL_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_BL_GPIO, 0);

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
        ESP_LOGE(TAG, "RGB panel create failed: %s", esp_err_to_name(err));
        return NULL;
    }
    esp_lcd_panel_reset(panel);

    void *fb = NULL;
    err = esp_lcd_rgb_panel_get_frame_buffer(panel, 1, &fb);
    if (err == ESP_OK && fb != NULL) {
        memset(fb, 0xFF, LCD_H_RES * LCD_V_RES * 2);
        esp_cache_msync(fb, LCD_H_RES * LCD_V_RES * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        ESP_LOGI(TAG, "FB=%p filled white", fb);
    }

    esp_lcd_panel_init(panel);
    ESP_LOGI(TAG, "Panel init complete");
    return panel;
}

void lcd_backlight_on(void)
{
    gpio_set_level(LCD_BL_GPIO, 1);
    ESP_LOGI(TAG, "backlight ON");
}

void *lcd_get_fb(esp_lcd_panel_handle_t panel)
{
    void *fb = NULL;
    esp_lcd_rgb_panel_get_frame_buffer(panel, 1, &fb);
    return fb;
}
