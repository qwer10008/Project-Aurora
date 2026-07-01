/*
 * Project Aurora — 主程序入口
 *
 * 职责：按顺序初始化各硬件模块 → 创建 LVGL 界面 → 主循环刷新时间
 *
 * 初始化顺序（有依赖关系的先初始化）：
 *   storage → rtc(I2C) → audio(I2S) → touch(I2C复用) → lcd → ui(LVGL)
 *
 * 主循环：
 *   每 500ms 读取一次 RTC 时间 → 格式化 → 更新 LVGL 标签 → lv_task_handler()
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

/* 项目组件接口 */
#include "storage.h"
#include "ds3231.h"
#include "audio.h"
#include "touch.h"
#include "lcd.h"
#include "ui.h"
#include "lvgl.h"

static const char *TAG = "aurora";

/* 时间格式化缓冲区 */
static char g_time_str[16];  /* "HH:MM:SS\0" = 9 字节，给 16 安全余量 */

void app_main(void)
{
    ESP_LOGI(TAG, "=== Project Aurora 启动 ===");

    /* ── Step 1: NVS 存储（最早，后续模块可能读写配置） ── */
    ESP_LOGI(TAG, "[1/6] storage_init...");
    storage_init();

    /* ── Step 2: RTC + I2C 总线 ── */
    ESP_LOGI(TAG, "[2/6] ds3231_init (I2C bus)...");
    ds3231_init();  /* 此函数安装 I2C_NUM_0 驱动，后续 I2C 设备复用此总线 */

    /* ── Step 3: 音频 I2S（独立，无前置依赖） ── */
    ESP_LOGI(TAG, "[3/6] audio_init...");
    audio_init();

    /* ── Step 4: 触摸（复用 I2C 总线，必须在 LCD 之前初始化） ── */
    ESP_LOGI(TAG, "[4/6] touch_init...");
    touch_init();
    /*
     * 为什么 touch 必须在 lcd 之前：
     * LCD 点亮后 I2C 总线上会产生噪声干扰 GT911 的初始化通信。
     */

    /* ── Step 5: LCD 屏幕 ── */
    ESP_LOGI(TAG, "[5/6] lcd_init...");
    esp_lcd_panel_handle_t panel = lcd_init();
    if (panel == NULL) {
        ESP_LOGE(TAG, "LCD 初始化失败，系统停止");
        return;
    }

    /* ── Step 6: LVGL 图形层（依赖 LCD 帧缓冲） ── */
    ESP_LOGI(TAG, "[6/6] ui_init...");
    ui_init(panel);

    /* ── 创建时钟显示界面 ── */
    lv_obj_t *time_label = ui_create_clock();
    if (time_label == NULL) {
        ESP_LOGE(TAG, "时钟界面创建失败");
        return;
    }

    /* 初始显示的时间（读 RTC） */
    rtc_time_t now;
    memset(&now, 0, sizeof(now));
    if (ds3231_read_time(&now)) {
        snprintf(g_time_str, sizeof(g_time_str),
                 "%02d:%02d:%02d", now.hour, now.min, now.sec);
        ui_update_time(time_label, g_time_str);
        ESP_LOGI(TAG, "初始时间: %s", g_time_str);
    } else {
        ESP_LOGW(TAG, "RTC 初始读取失败，显示默认时间");
        ui_update_time(time_label, "--:--:--");
    }

    /* 打开背光——LVGL 第一帧已在帧缓冲中 */
    lcd_backlight_on();

    /* ── 主循环：500ms 周期刷新 ── */
    int loop_count = 0;
    while (1) {
        /* 每 500ms 读取一次 RTC 更新时间显示 */
        if (ds3231_read_time(&now)) {
            snprintf(g_time_str, sizeof(g_time_str),
                     "%02d:%02d:%02d", now.hour, now.min, now.sec);
            ui_update_time(time_label, g_time_str);

            /* 每 60 次（约 30 秒）打印一次日志，避免刷屏 */
            if (loop_count % 60 == 0) {
                ESP_LOGI(TAG, "时间: %s", g_time_str);
            }
        }

        /* 让 LVGL 处理待渲染的区域并调用 flush 回调 */
        lv_task_handler();

        loop_count++;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
