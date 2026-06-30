#include "../components/drv_lcd/lcd.h"
#include "../components/drv_rtc/rtc.h"
#include "../components/drv_audio/audio.h"
#include "../components/drv_touch/touch.h"
#include "../components/drv_storage/storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static esp_lcd_panel_handle_t panel = NULL;

// 在帧缓冲正中绘制时间字符串（简易 bitmap 字体，纯 CPU 渲染）
static void draw_time(const rtc_time_t *t)
{
    void *fb = lcd_get_fb(panel);
    if (fb == NULL) return;

    char str[32];
    snprintf(str, sizeof(str), "%02d:%02d:%02d", t->hour, t->min, t->sec);

    // 清屏填白
    uint16_t *buf = (uint16_t *)fb;
    for (int i = 0; i < 800 * 480; i++) buf[i] = 0xFFFF;

    // 在底部画一条状态栏：如果有保存的闹钟则显示
    uint16_t alarm = storage_load_alarm();
    char info[64];
    snprintf(info, sizeof(info), "闹钟 %02d:%02d", alarm / 60, alarm % 60);

    // 用 printf 在终端同步打印（LVGL 之前先这样）
    printf("\r%s  |  %s", str, info);
    fflush(stdout);
}

void app_main(void)
{
    // ---- 初始化所有硬件 ----
    storage_init();
    ds3231_init();
    audio_init();
    touch_init();     // ★ 移到 LCD 之前——避开 RGB 噪声
    panel = lcd_init();

    // ---- 检查 NVS 首次启动 ----
    rtc_time_t t = {0};
    if (!ds3231_read_time(&t)) {
        // DS3231M 未设时间，写一个默认值
        t = (rtc_time_t){0, 30, 7, 30, 6, 26};  // 7:30 2026-06-30
        ds3231_write_time(&t);
    }

    printf("\n=== Project Aurora 闹钟已启动 ===\n");

    uint16_t alarm_min = storage_load_alarm();
    int last_sec = -1;
    uint32_t alarm_playing_since = 0;

    while (1) {
        if (!ds3231_read_time(&t)) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        // 屏幕刷新 — 秒变化时才重绘
        if (t.sec != last_sec) {
            last_sec = t.sec;
            draw_time(&t);
        }

        // 闹钟触发：当前时间(时*60+分) == 设定闹钟分钟数，且秒=0
        uint16_t now_min = t.hour * 60 + t.min;
        if (now_min == alarm_min && t.sec == 0 && alarm_playing_since == 0) {
            alarm_playing_since = 1;
            printf("\n*** 闹钟响了！ ***\n");
            audio_play_tone(880, 500);
        }

        // 触摸关闭闹钟
        uint16_t tx, ty;
        if (touch_read(&tx, &ty)) {
            if (alarm_playing_since > 0) {
                alarm_playing_since = 0;
                printf("\n*** 闹钟已关闭 ***\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
