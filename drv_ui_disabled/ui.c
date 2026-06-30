/*
 * drv_ui — LVGL 图形库与 LCD + 触摸的对接层
 *
 * 职责：
 *   1. 分配 PSRAM 渲染缓冲，LVGL 在里面画好，flush 回调搬到 LCD 帧缓冲
 *   2. 把 GT911 触摸数据喂给 LVGL 输入系统
 *   3. 启动 5ms 滴答定时器，驱动 LVGL 内部动画和定时器
 *
 * PARTIAL 模式：LVGL 用自己的缓冲渲染，不直接写 LCD 帧缓冲
 *   优势：更稳定，避免 LVGL 写帧缓冲时和 LCD 控制器读帧缓冲冲突
 *   代价：多耗 ~150KB PSRAM（两帧 800×48 的缓冲）+ flush 时 memcpy 开销
 */
#include "ui.h"
#include "lcd.h"
#include "touch.h"
#include "lvgl.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_cache.h"          // esp_cache_msync()——CPU 写完帧缓冲后刷新缓存到 PSRAM
#include <stdlib.h>              // malloc()
#include <string.h>              // memcpy()

static const char *TAG = "ui";

#define LCD_W           800    // 屏幕宽度
#define BUF_LINES       48     // 每个渲染缓冲覆盖的行数
#define PX_BYTES        2      // RGB565 每像素 2 字节（不加 alpha）
#define BUF_SIZE        (LCD_W * BUF_LINES * PX_BYTES)  // 单缓冲字节数

static uint16_t *g_fb = NULL;  // 缓存的 LCD 帧缓冲首地址（PSRAM），flush 回调里用
static int g_flush_cnt = 0;    // flush 调用计数器（调试用）

// === LVGL flush 回调（PARTIAL 模式）===
// LVGL 在自己的缓冲里画好了一小块像素 → 我们逐行拷贝到 LCD 帧缓冲
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    // 前 10 次 flush 打印日志，确认回调是否被调用
    if (g_flush_cnt < 10) {
        ESP_LOGI(TAG, "flush #%d: (%d,%d)-(%d,%d) w=%d h=%d",
                 g_flush_cnt, area->x1, area->y1, area->x2, area->y2,
                 area->x2 - area->x1 + 1, area->y2 - area->y1 + 1);
        g_flush_cnt++;
    }

    uint16_t *src = (uint16_t *)px_map;  // 源 = LVGL 内部缓冲

    // 逐行拷贝：area->y1 到 area->y2 是需要刷新的行范围
    for (int y = area->y1; y <= area->y2; y++) {
        int dst_off = y * LCD_W + area->x1;           // 目标 = 帧缓冲第 y 行、第 x1 列
        int line_w  = area->x2 - area->x1 + 1;        // 这一行的像素宽度
        memcpy(&g_fb[dst_off], src, line_w * sizeof(uint16_t));
        src += line_w;                                 // 源指针跳到下一行
    }

    // 关键：CPU 写帧缓冲走的是 cache，必须刷新到 PSRAM 物理内存
    // 否则 LCD 控制器通过 DMA 读 PSRAM 时会看到旧数据（随机值 → 花屏/黑屏）
    void *flush_start = &g_fb[area->y1 * LCD_W];           // 受影响的首行起始地址
    size_t flush_size = (area->y2 - area->y1 + 1) * LCD_W * sizeof(uint16_t);
    esp_cache_msync(flush_start, flush_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    lv_display_flush_ready(disp);  // 通知 LVGL 本轮搬运完成
}

// === LVGL 触摸读取回调 ===
static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t x, y;
    if (touch_read(&x, &y)) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// === LVGL 滴答定时器回调 ===
static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(5);  // 每 5ms 调用一次，让 LVGL 知道时间流逝
}

// === 公开：初始化 LVGL ===
void ui_init(esp_lcd_panel_handle_t panel)
{
    // 1. 初始化 LVGL 内核
    ESP_LOGI(TAG, "lv_init...");
    lv_init();

    // 2. 创建显示对象，强制设为 RGB565（与 LCD 帧缓冲格式一致）
    ESP_LOGI(TAG, "创建显示 800x480...");
    lv_display_t *disp = lv_display_create(800, 480);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);  // 纯 RGB565，不用 Alpha
    ESP_LOGI(TAG, "显示颜色格式: %d (期望 16=RGB565)", (int)lv_display_get_color_format(disp));

    // 3. 在 PSRAM 中分配两个渲染缓冲（PARTIAL 双缓冲）
    ESP_LOGI(TAG, "分配 PSRAM 渲染缓冲 (%d bytes x2)...", BUF_SIZE);
    void *buf1 = malloc(BUF_SIZE);
    void *buf2 = malloc(BUF_SIZE);
    if (buf1 == NULL || buf2 == NULL) {
        ESP_LOGE(TAG, "缓冲分配失败！检查 PSRAM 是否启用");
        return;
    }

    // PARTIAL 模式：LVGL 用 buf1/buf2 轮换渲染，每画完一块就调 flush_cb
    lv_display_set_buffers(disp, buf1, buf2, BUF_SIZE,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    // 4. 缓存 LCD 帧缓冲地址（flush 回调里用 lcd_get_fb 每次查太慢）
    g_fb = (uint16_t *)lcd_get_fb(panel);
    if (g_fb == NULL) {
        ESP_LOGE(TAG, "获取 LCD 帧缓冲失败！");
        return;
    }
    ESP_LOGI(TAG, "LCD 帧缓冲: %p", (void *)g_fb);

    // 5. 注册 flush 回调
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    // 6. 创建触摸输入设备
    ESP_LOGI(TAG, "注册触摸输入...");
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_read_cb);

    // 7. 启动 5ms 滴答定时器
    ESP_LOGI(TAG, "启动 5ms 滴答定时器...");
    const esp_timer_create_args_t timer_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t tick_timer;
    esp_timer_create(&timer_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, 5000);  // 5000 微秒 = 5 毫秒

    ESP_LOGI(TAG, "LVGL 初始化完成（PARTIAL 模式, %d 行缓冲）", BUF_LINES);
}
