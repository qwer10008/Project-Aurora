/*
 * Project Aurora — 智能闹钟
 * main.c：程序入口，负责初始化所有硬件模块，然后循环读取时间、刷新屏幕、
 *         检测闹钟触发、响应触摸关闭。
 *
 * 本项目采用"组件化"结构：
 *   main/          → 只管调度（本文件）
 *   components/    → 每个硬件模块一个文件夹，里面有 .h（接口声明）和 .c（实现）
 *
 * 这样 main.c 只需要调用各模块提供的函数，不需要关心底层细节。
 */

/* === 头文件 === */
#include "../components/drv_lcd/lcd.h"       // LCD 屏幕：lcd_init()、lcd_get_fb()
#include "../components/drv_rtc/rtc.h"       // DS3231M 时钟：ds3231_init()、ds3231_read_time()、ds3231_write_time()
#include "../components/drv_audio/audio.h"   // MAX98357A 喇叭：audio_init()、audio_play_tone()
#include "../components/drv_touch/touch.h"   // GT911 触摸：touch_init()、touch_read()
#include "../components/drv_storage/storage.h" // NVS 存储：storage_init()、storage_save_alarm()、storage_load_alarm()
#include <stdio.h>   // printf()
#include <string.h>  // snprintf()

/* === 全局变量 === */

// panel 是 LCD 的"句柄"——后续操作屏幕（比如获取帧缓冲）都需要传这个变量
// 设为全局是因为 app_main() 和 draw_time() 都要用到它
static esp_lcd_panel_handle_t panel = NULL;

/* === 函数定义 === */

/*
 * draw_time() — 在屏幕上显示当前时间和闹钟设定
 *
 * 当前阶段没有 LVGL 字体，用最原始的方式：
 *   1. 获取帧缓冲（一整块内存，每个像素占 2 字节 RGB565）
 *   2. 全部填白色
 *   3. 同时在串口终端打印时间（屏幕上暂时看不到字，但终端能看到）
 *
 * 参数：
 *   t         — 当前时间（从 DS3231M 读出的）
 *   alarm_min — 闹钟设定的分钟数（0-1439，例如 7:00 = 420）
 */
static void draw_time(const rtc_time_t *t, uint16_t alarm_min)
{
    // 从 LCD 拿到帧缓冲指针——指向 PSRAM 里一整块 800×480 像素的内存
    void *fb = lcd_get_fb(panel);
    if (fb == NULL) return;  // 拿不到就不画（LCD 还没初始化好等情况）

    // 把帧缓冲转成 uint16_t 数组——因为 RGB565 格式每个像素正好 2 字节 = 1 个 uint16_t
    uint16_t *buf = (uint16_t *)fb;

    // 全部像素填 0xFFFF（白色），清空上一帧的内容
    for (int i = 0; i < 800 * 480; i++) {
        buf[i] = 0xFFFF;  // RGB565 的白色 = 红绿蓝全满
    }

    // 构建时间字符串，例如 "07:30:45"
    char str[32];
    snprintf(str, sizeof(str), "%02d:%02d:%02d",
             t->hour, t->min, t->sec);

    // 构建闹钟信息字符串，例如 "Alarm 07:00"
    char info[64];
    snprintf(info, sizeof(info), "Alarm %02d:%02d",
             alarm_min / 60,    // 分钟 → 小时（整除 60）
             alarm_min % 60);   // 分钟 → 余数（取模 60）

    // 终端打印（LVGL 就位前，我们先用终端确认程序在跑）
    // \r = 回到行首，这样同一行反复刷新，不会刷屏
    printf("\r%s  |  %s", str, info);
    fflush(stdout);  // 强制输出（printf 默认有缓冲，不及时刷新终端看不到）
}

/* === 主函数 === */
void app_main(void)
{
    // ---- 第 1 步：初始化所有硬件模块（顺序有讲究） ----

    storage_init();   // 1. NVS 存储——最早初始化，后续组件可能需要读写配置
    ds3231_init();    // 2. DS3231M 时钟——顺带初始化了 I2C 总线（GPIO41+42）
    audio_init();     // 3. MAX98357A 喇叭——用独立的 I2S 接口（GPIO4/5/7），不依赖 I2C
    touch_init();     // 4. GT911 触摸——必须在 LCD 之前初始化！LCD 亮起后 I2C 总线上有噪声
    panel = lcd_init(); // 5. LCD 屏幕——最后初始化，顺带把背光点亮

    // ---- 第 2 步：检查 DS3231M 是否已设置过时间 ----

    rtc_time_t t;  // 时间结构体：包含 sec/min/hour/day/month/year
    if (!ds3231_read_time(&t)) {
        // 读取失败（芯片出厂后第一次上电，寄存器里是乱码）
        // 写入一个默认时间，防止后续读到垃圾值
        // C99 复合字面量语法：(rtc_time_t){秒, 分, 时, 日, 月, 年}
        t = (rtc_time_t){0, 30, 7, 30, 6, 26};  // 2026-06-30 07:30:00
        ds3231_write_time(&t);
    }

    // ---- 第 3 步：从 NVS 读取上次保存的闹钟时间 ----

    uint16_t alarm_min = storage_load_alarm();  // 返回 0-1439 的分钟数，默认 420（7:00）
    int last_sec = -1;     // 记录上一次刷新的秒数，秒不变就不重复画
    bool alarm_active = false;  // 闹钟是否正在响

    printf("\n=== Project Aurora ===\n");

    // ---- 第 4 步：主循环（永远不会退出） ----

    while (1) {
        // 从 DS3231M 读取当前时间
        if (!ds3231_read_time(&t)) {
            // 读取失败（I2C 偶尔受 LCD 干扰），等 200ms 再试
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;  // 跳过本次循环，重新读
        }

        // 秒数变化了 → 刷新屏幕显示
        if (t.sec != last_sec) {
            last_sec = t.sec;
            draw_time(&t, alarm_min);  // 画白色背景 + 终端打印时间
        }

        // 闹钟检测：当前时间(时*60+分) == 设定闹钟时间，且秒==0（只在整分钟触发一次）
        uint16_t now = t.hour * 60 + t.min;
        if (now == alarm_min && t.sec == 0 && !alarm_active) {
            alarm_active = true;
            printf("\n*** ALARM ***\n");
            audio_play_tone(880, 500);  // 播放 880Hz 正弦波，持续 500ms
        }

        // 触摸检测：如果有触摸事件且闹钟正在响 → 关闭闹钟
        uint16_t tx, ty;  // 触摸坐标（暂未用到，先接收）
        if (touch_read(&tx, &ty) && alarm_active) {
            alarm_active = false;
            printf("\n*** 闹钟已关闭 ***\n");
        }

        // 休眠 100ms 再进入下一轮循环（避免 while(1) 空转烧 CPU）
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
