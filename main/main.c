/*
 * Project Aurora — 智能闹钟
 * main.c：硬件初始化 → UI 初始化 → 主循环（LVGL 驱动 + RTC 读取 + 闹钟 + 触摸）
 *
 * 初始化顺序（每一步都有讲究）：
 *   1. storage_init()     → NVS，最早，后续组件可能读写配置
 *   2. ds3231_init()       → I2C 总线（GPIO41+42），触摸和 RTC 共用
 *   3. audio_init()        → I2S，独立接口，不受 I2C/LCD 影响
 *   4. touch_init()        → GT911（I2C），必须在 LCD 之前！
 *   5. lcd_init()          → RGB 面板，背光暂不点亮
 *   6. ui_init(panel)      → LVGL 初始化 + 渲染首帧到帧缓冲
 *   7. lcd_backlight_on()  → 首帧就绪，点亮背光！
 */

/* === 头文件 === */
#include "../components/drv_lcd/lcd.h"           // lcd_init()、lcd_get_fb()、lcd_backlight_on()
#include "../components/drv_rtc/rtc.h"           // ds3231_init()、ds3231_read_time()
#include "../components/drv_audio/audio.h"       // audio_init()、audio_play_tone()
#include "../components/drv_touch/touch.h"       // touch_init()、touch_read()
#include "../components/drv_storage/storage.h"   // storage_init()、storage_load_alarm()
#include "../components/drv_ui/ui.h"             // ui_init()
#include "lvgl.h"                                // lv_label_create()、lv_timer_handler() 等
#include "esp_log.h"                             // ESP_LOGI()、ESP_LOGE()
#include "freertos/FreeRTOS.h"                   // vTaskDelay()
#include "freertos/task.h"                       // pdMS_TO_TICKS()
#include <stdio.h>                               // printf()、snprintf()
#include <string.h>                              // snprintf()

static const char *TAG = "main";
static esp_lcd_panel_handle_t panel = NULL;      // LCD 面板句柄

/* === 主函数 === */
void app_main(void)
{
    // ==== 第 1 步：初始化硬件 ====
    ESP_LOGI(TAG, "storage_init...");
    storage_init();               // NVS——最早

    ESP_LOGI(TAG, "ds3231_init...");
    ds3231_init();                 // I2C 总线 + DS3231M

    ESP_LOGI(TAG, "audio_init...");
    audio_init();                  // I2S 喇叭

    ESP_LOGI(TAG, "touch_init...");
    touch_init();                  // GT911 触摸（I2C，必须在 LCD 之前！）

    ESP_LOGI(TAG, "lcd_init...");
    panel = lcd_init();            // RGB 面板（背光此时还是灭的）

    // ==== 第 2 步：LVGL 初始化 + 画首帧 ====
    ESP_LOGI(TAG, "ui_init...");
    ui_init(panel);                // LVGL 接管帧缓冲，渲染默认界面

    // ==== 第 3 步：创建 UI 控件（在首帧里）====
    lv_obj_t *scr = lv_screen_active();

    lv_obj_t *time_label = lv_label_create(scr);
    lv_label_set_text(time_label, "00:00:00");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(time_label, lv_color_black(), 0);
    lv_obj_center(time_label);

    lv_obj_t *alarm_label = lv_label_create(scr);
    lv_label_set_text(alarm_label, "Alarm 07:00");
    lv_obj_set_style_text_font(alarm_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(alarm_label, lv_color_black(), 0);
    lv_obj_align(alarm_label, LV_ALIGN_BOTTOM_MID, 0, -30);

    // ==== 第 4 步：强制 LVGL 渲染首帧到帧缓冲（背光还没亮）====
    ESP_LOGI(TAG, "渲染首帧...");
    lv_timer_handler();           // LVGL 把标签画到帧缓冲
    lv_timer_handler();           // 再刷一轮确保 flush 完成

    // ==== 第 5 步：点亮背光，再刷一帧确保稳定 ====
    ESP_LOGI(TAG, "lcd_backlight_on...");
    lcd_backlight_on();
    lv_timer_handler();           // 背光点亮后再刷一次
    lv_timer_handler();           // 再来一次确保 flush 完成
    rtc_time_t t;
    if (!ds3231_read_time(&t)) {
        t = (rtc_time_t){0, 30, 7, 30, 6, 26};  // 2026-06-30 07:30:00
        ds3231_write_time(&t);
    }

    printf("\n=== Project Aurora ===\n");
    ESP_LOGI(TAG, "进入主循环（最简测试：只跑 LVGL，不读 RTC）");

    // ==== 最简测试主循环 ====
    // 目的：确认 LVGL 渲染能否稳定保持，排除 RTC/闹钟/触摸逻辑的干扰
    int loop_count = 0;
    while (1) {
        lv_timer_handler();          // 驱动 LVGL
        vTaskDelay(pdMS_TO_TICKS(5)); // 5ms 间隔

        // 每 200 次循环（约 1 秒）打印一次心跳，确认主循环没死
        if (++loop_count % 200 == 0) {
            printf(".");
            fflush(stdout);
        }
    }
}
