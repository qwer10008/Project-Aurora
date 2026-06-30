#include "../components/drv_lcd/lcd.h"
#include "../components/drv_rtc/rtc.h"
#include "../components/drv_audio/audio.h"
#include "../components/drv_touch/touch.h"
#include "../components/drv_storage/storage.h"
#include <stdio.h>
#include <string.h>

static esp_lcd_panel_handle_t panel = NULL;

static void draw_time(const rtc_time_t *t, uint16_t alarm_min)
{
    void *fb = lcd_get_fb(panel);
    if (fb == NULL) return;

    uint16_t *buf = (uint16_t *)fb;
    // 白色背景
    for (int i = 0; i < 800 * 480; i++) buf[i] = 0xFFFF;

    char str[32];
    snprintf(str, sizeof(str), "%02d:%02d:%02d", t->hour, t->min, t->sec);
    char info[64];
    snprintf(info, sizeof(info), "Alarm %02d:%02d", alarm_min / 60, alarm_min % 60);
    printf("\r%s  |  %s", str, info);
    fflush(stdout);
}

void app_main(void)
{
    storage_init();
    ds3231_init();
    audio_init();
    touch_init();     // LCD 之前初始化（避开 RGB 噪声）
    panel = lcd_init();

    // 首次写 RTC 默认时间
    rtc_time_t t;
    if (!ds3231_read_time(&t)) {
        t = (rtc_time_t){0, 30, 7, 30, 6, 26};
        ds3231_write_time(&t);
    }

    uint16_t alarm_min = storage_load_alarm();
    int last_sec = -1;
    bool alarm_active = false;

    printf("\n=== Project Aurora ===\n");

    while (1) {
        if (!ds3231_read_time(&t)) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        // 每秒刷新显示
        if (t.sec != last_sec) {
            last_sec = t.sec;
            draw_time(&t, alarm_min);
        }

        // 闹钟触发
        uint16_t now = t.hour * 60 + t.min;
        if (now == alarm_min && t.sec == 0 && !alarm_active) {
            alarm_active = true;
            printf("\n*** ALARM ***\n");
            audio_play_tone(880, 500);
        }

        // 触摸关闭
        uint16_t tx, ty;
        if (touch_read(&tx, &ty) && alarm_active) {
            alarm_active = false;
            printf("\n*** 闹钟已关闭 ***\n");
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
