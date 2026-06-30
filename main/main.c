/*
 * Project Aurora — 智能闹钟
 * main.c：硬件初始化 → UI 初始化 → 主循环（LVGL 驱动 + RTC 读取 + 闹钟 + 触摸）
 *
 * 组件依赖：
 *   drv_storage → NVS 闹钟持久化
 *   drv_rtc     → DS3231M 时间读取 + I2C 总线
 *   drv_audio   → MAX98357A 闹钟音播放
 *   drv_touch   → GT911 触摸检测
 *   drv_lcd     → ST7262 RGB 屏幕
 *   drv_ui      → LVGL 初始化 + LCD flush 对接 + 触摸输入对接
 */

/* === 头文件 === */
#include "../components/drv_lcd/lcd.h"       // LCD 屏幕：lcd_init()
#include "../components/drv_rtc/rtc.h"       // DS3231M 时钟：ds3231_init()、ds3231_read_time()
#include "../components/drv_audio/audio.h"   // MAX98357A 喇叭：audio_init()、audio_play_tone()
#include "../components/drv_touch/touch.h"   // GT911 触摸：touch_init()、touch_read()
#include "../components/drv_storage/storage.h" // NVS 存储：storage_init()、storage_load_alarm()
#include "../components/drv_ui/ui.h"         // LVGL 对接：ui_init()
#include "lvgl.h"                            // LVGL 控件 API：lv_label_create()、lv_timer_handler() 等
#include "freertos/FreeRTOS.h"               // vTaskDelay()
#include "freertos/task.h"                   // pdMS_TO_TICKS()
#include <stdio.h>                           // printf()、snprintf()
#include <string.h>                          // snprintf()

/* === 全局变量 === */
static esp_lcd_panel_handle_t panel = NULL;  // LCD 面板句柄（传递给 lcd_get_fb）

/* === 主函数 === */
void app_main(void)
{
    // ---- 第 1 步：初始化所有硬件模块（顺序有讲究） ----
    storage_init();       // 1. NVS 存储——最早初始化，后续组件可能读写配置
    ds3231_init();         // 2. DS3231M 时钟——顺带初始化了 I2C 总线（GPIO41+42）
    audio_init();          // 3. MAX98357A 喇叭——独立的 I2S 接口，不依赖 I2C
    touch_init();          // 4. GT911 触摸——必须在 LCD 之前！LCD 亮起后 I2C 总线有噪声
    panel = lcd_init();    // 5. LCD 屏幕——最后初始化，点亮背光 + 创建帧缓冲

    // ---- 第 2 步：初始化 LVGL，接管 LCD 帧缓冲作为画布 ----
    ui_init(panel);
    // 从此刻起，屏幕显示由 LVGL 控制。之前 lcd_init() 画的白屏会被 LVGL 覆盖。

    // ---- 第 3 步：在屏幕上创建 UI 控件 ----
    lv_obj_t *scr = lv_screen_active();          // 获取当前活动屏幕（LVGL v9 API）

    // 时间标签——大号数字，屏幕居中
    lv_obj_t *time_label = lv_label_create(scr);
    lv_label_set_text(time_label, "00:00:00");   // 初始文字（时间还没读到）
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(time_label, lv_color_black(), 0);
    lv_obj_center(time_label);                   // 屏幕正中间

    // 闹钟信息标签——屏幕底部居中
    lv_obj_t *alarm_label = lv_label_create(scr);
    lv_label_set_text(alarm_label, "Alarm 07:00");
    lv_obj_set_style_text_font(alarm_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(alarm_label, lv_color_black(), 0);
    lv_obj_align(alarm_label, LV_ALIGN_BOTTOM_MID, 0, -30);  // 距底部 30px

    // ---- 第 4 步：检查 DS3231M 是否已设置过时间 ----
    rtc_time_t t;  // 时间结构体：sec/min/hour/day/month/year
    if (!ds3231_read_time(&t)) {
        // 读取失败（芯片出厂后第一次上电，寄存器是乱码）
        // C99 复合字面量：{秒, 分, 时, 日, 月, 年}
        t = (rtc_time_t){0, 30, 7, 30, 6, 26};  // 2026-06-30 07:30:00
        ds3231_write_time(&t);
    }

    // ---- 第 5 步：从 NVS 读取闹钟时间 ----
    uint16_t alarm_min = storage_load_alarm();    // 返回 0~1439 分钟数，默认 420（7:00）
    int last_sec = -1;                            // 上次刷新的秒数（初始 -1 确保首次一定刷新）
    bool alarm_active = false;                    // 闹钟是否正在响（true=正在响）
    uint32_t last_rtc_check = 0;                  // 上次读 RTC 时的 LVGL 滴答值（毫秒）

    printf("\n=== Project Aurora ===\n");

    // ---- 第 6 步：主循环（永不退出） ----
    while (1) {
        // ① LVGL 定时器——必须高频调用（~每 5ms），驱动 UI 事件、动画、触摸响应
        lv_timer_handler();

        // ② RTC + 闹钟逻辑（每 ~100ms 执行一次，用 LVGL 滴答计时）
        //    lv_tick_get() 返回 LVGL 内部毫秒计数（由 esp_timer 驱动）
        uint32_t tick_now = lv_tick_get();
        if (tick_now - last_rtc_check >= 100) {  // 距上次检查已过 100ms
            last_rtc_check = tick_now;

            // 读取 DS3231M 时间，失败则跳过本轮（等下一轮再试）
            if (ds3231_read_time(&t)) {

                // 秒数变化 → 更新屏幕 + 终端打印
                if (t.sec != last_sec) {
                    last_sec = t.sec;

                    // 更新 LVGL 时间标签
                    char str[32];
                    snprintf(str, sizeof(str), "%02d:%02d:%02d",  // %02d=两位数字，不够补零
                             t.hour, t.min, t.sec);
                    lv_label_set_text(time_label, str);

                    // 更新闹钟标签
                    char info[64];
                    snprintf(info, sizeof(info), "Alarm %02d:%02d",
                             alarm_min / 60, alarm_min % 60);
                    lv_label_set_text(alarm_label, info);

                    // 终端同步打印
                    printf("\r%s  |  %s", str, info);
                    fflush(stdout);
                }

                // 闹钟检测：当前分钟 == 设定分钟 且 秒==0（整分钟触发一次）
                uint16_t now = t.hour * 60 + t.min;
                if (now == alarm_min && t.sec == 0 && !alarm_active) {
                    alarm_active = true;
                    printf("\n*** ALARM ***\n");
                    // 播放 880Hz 提示音 500ms（阻塞）——这是基础闹钟行为
                    // 后续会改为持续响铃 + LVGL 按钮关闭
                    audio_play_tone(880, 500);
                }

                // 触摸关闭闹钟：有触摸 + 闹钟在响 → 关闭
                uint16_t tx, ty;
                if (touch_read(&tx, &ty) && alarm_active) {
                    alarm_active = false;
                    printf("\n*** 闹钟已关闭 ***\n");
                }
            }
        }

        // ③ 休眠 5ms 让出 CPU——LVGL 需要高频轮询，所以这里比原来 100ms 短很多
        //    pdMS_TO_TICKS(5) = 把 5 毫秒换算成 FreeRTOS 的 tick 数
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
