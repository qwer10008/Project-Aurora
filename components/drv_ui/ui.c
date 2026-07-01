/*
 * drv_ui — LVGL layer, restored to the WORKING version + mutex fix
 *
 * FULL mode, esp_lcd_panel_draw_bitmap(), on_color_trans_done,
 * keep-alive timer, mutex-protected LVGL task.
 */
#include "ui.h"
#include "lcd.h"
#include "touch.h"
#include "lvgl.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ui";

#define LCD_W           800
#define LCD_H           480
#define PX_BYTES        2
#define FB_SIZE         (LCD_W * LCD_H * PX_BYTES)

static esp_lcd_panel_handle_t g_panel = NULL;
static esp_timer_handle_t g_tick_timer = NULL;
static int g_flush_cnt = 0;
static SemaphoreHandle_t g_lvgl_mutex = NULL;

/* ────────── Color transfer done ────────── */
static bool on_color_trans_done(esp_lcd_panel_handle_t panel,
                                const esp_lcd_rgb_panel_event_data_t *edata,
                                void *user_ctx)
{
    lv_display_flush_ready((lv_display_t *)user_ctx);
    return false;
}

/* ────────── Flush callback ────────── */
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if (g_flush_cnt < 5 || g_flush_cnt % 100 == 0) {
        ESP_LOGI(TAG, "flush #%d | (%d,%d)-(%d,%d) %dx%d",
                 g_flush_cnt, area->x1, area->y1, area->x2, area->y2,
                 area->x2 - area->x1 + 1, area->y2 - area->y1 + 1);
    }
    g_flush_cnt++;
    esp_lcd_panel_draw_bitmap(g_panel, area->x1, area->y1,
                              area->x2 + 1, area->y2 + 1, px_map);
}

/* ────────── Touch ────────── */
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

/* ────────── Tick ────────── */
static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(5);
}

static void keepalive_timer_cb(lv_timer_t *timer)
{
    lv_obj_invalidate(lv_scr_act());
}

/* ────────── LVGL task ────────── */
static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL task started");
    while (1) {
        if (xSemaphoreTake(g_lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            uint32_t delay_ms = lv_timer_handler();
            xSemaphoreGive(g_lvgl_mutex);
            if (delay_ms < 1) delay_ms = 1;
            if (delay_ms > 500) delay_ms = 500;
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }
}

/* ────────── Public: init ────────── */
void ui_init(esp_lcd_panel_handle_t panel)
{
    g_panel = panel;
    g_lvgl_mutex = xSemaphoreCreateMutex();

    ESP_LOGI(TAG, "lv_init...");
    lv_init();

    ESP_LOGI(TAG, "Creating display 800x480...");
    lv_display_t *disp = lv_display_create(LCD_W, LCD_H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    ESP_LOGI(TAG, "Color format: %d (0x12=RGB565)", (int)lv_display_get_color_format(disp));

    esp_lcd_rgb_panel_event_callbacks_t cbs = {.on_color_trans_done = on_color_trans_done};
    esp_lcd_rgb_panel_register_event_callbacks(panel, &cbs, disp);

    ESP_LOGI(TAG, "Allocating draw buffer (%d bytes)...", FB_SIZE);
    void *draw_buf = malloc(FB_SIZE);
    if (draw_buf == NULL) { ESP_LOGE(TAG, "draw buffer alloc failed!"); return; }
    memset(draw_buf, 0xFF, FB_SIZE);

    lv_display_set_buffers(disp, draw_buf, NULL, FB_SIZE,
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    ESP_LOGI(TAG, "Registering touch input...");
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_read_cb);

    ESP_LOGI(TAG, "Starting 5ms tick timer...");
    const esp_timer_create_args_t ta = {.callback = lvgl_tick_cb, .name = "lvgl_tick"};
    esp_timer_create(&ta, &g_tick_timer);
    esp_timer_start_periodic(g_tick_timer, 5000);

    lv_timer_create(keepalive_timer_cb, 30, NULL);
    ESP_LOGI(TAG, "LVGL initialized");
}

/* ────────── Public: start task ────────── */
void ui_start_task(void)
{
    xTaskCreate(lvgl_task, "lvgl", 4096, NULL, 5, NULL);
}

/* ────────── Public: create clock UI ────────── */
lv_obj_t *ui_create_clock(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "--:--:--");
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
    lv_obj_center(label);

    ESP_LOGI(TAG, "Clock UI created");
    return label;
}

/* ────────── Public: update time ────────── */
void ui_update_time(lv_obj_t *label, const char *time_str)
{
    if (label == NULL || time_str == NULL) return;
    if (xSemaphoreTake(g_lvgl_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        lv_label_set_text(label, time_str);
        xSemaphoreGive(g_lvgl_mutex);
    }
}
