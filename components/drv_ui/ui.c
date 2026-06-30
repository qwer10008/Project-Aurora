/*
 * drv_ui — LVGL 图形库与 LCD + 触摸的对接层
 *
 * 职责：
 *   1. 把 LCD 帧缓冲交给 LVGL 当画布（DIRECT 模式，零拷贝）
 *   2. 把 GT911 触摸数据喂给 LVGL 输入系统
 *   3. 启动 5ms 滴答定时器，驱动 LVGL 内部动画和定时器
 */
#include "ui.h"
#include "lcd.h"
#include "touch.h"
#include "lvgl.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "ui";

// === LVGL flush 回调 ===
// LVGL 每渲染完一个区域就调用此函数，通知我们"这块画好了"
// DIRECT 模式下，像素数据已经在帧缓冲里了，不需要搬运，直接标记完成
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    lv_display_flush_ready(disp);  // 告诉 LVGL 本次刷新完成
}

// === LVGL 触摸读取回调 ===
// LVGL 定期调用此函数来检查触摸屏状态
static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t x, y;
    if (touch_read(&x, &y)) {
        data->state = LV_INDEV_STATE_PRESSED;  // 手指按下
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED; // 手指抬起
    }
}

// === LVGL 滴答定时器回调 ===
// 硬件定时器每 5ms 触发一次，调用 lv_tick_inc() 让 LVGL 知道时间流逝
static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(5);  // 告诉 LVGL 又过了 5 毫秒
}

// === 公开：初始化 LVGL ===
void ui_init(esp_lcd_panel_handle_t panel)
{
    // 第 1 步：初始化 LVGL 内部数据结构（内存池、样式引擎等）
    lv_init();

    // 第 2 步：创建 LVGL 显示对象，指定分辨率 800×480
    lv_display_t *disp = lv_display_create(800, 480);

    // 第 3 步：拿 LCD 帧缓冲给 LVGL 当画布
    // DIRECT 模式 = LVGL 直接在帧缓冲里画，不用中间缓冲区
    // buf2 = NULL → 单缓冲（画面更新时可能有轻微撕裂，但省内存）
    void *fb = lcd_get_fb(panel);
    lv_display_set_buffers(disp, fb, NULL,
                           800 * 480 * sizeof(lv_color_t),
                           LV_DISPLAY_RENDER_MODE_DIRECT);

    // 第 4 步：注册 flush 回调——LVGL 画完一块就回调一次
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    // 第 5 步：创建触摸输入设备，告诉 LVGL 怎么读触摸坐标
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_read_cb);

    // 第 6 步：启动 5ms 滴答定时器（esp_timer 是硬件高精度定时器）
    // 周期 5000 微秒 = 5 毫秒，LVGL 需要这个来驱动动画和内部定时器
    const esp_timer_create_args_t timer_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t tick_timer;
    esp_timer_create(&timer_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, 5000);  // 5000μs = 5ms

    ESP_LOGI(TAG, "LVGL 初始化完成（800x480 DIRECT 模式 + 触摸输入）");
}
