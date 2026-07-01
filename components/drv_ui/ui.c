/*
 * drv_ui — LVGL graphics layer for LCD + touch
 *
 * Architecture:
 *   PARTIAL mode: LVGL renders into PSRAM buffers, then flush callback
 *   copies each rendered chunk to the LCD frame buffer. Cache is flushed
 *   after each copy so LCD DMA sees fresh data.
 *
 * Dependencies:
 *   drv_lcd   — lcd_get_fb() for the frame buffer address
 *   drv_touch — touch_read() feeds LVGL input subsystem
 *   esp_timer — 5ms periodic tick drives LVGL internal clock
 */
#include "ui.h"
#include "lcd.h"
#include "touch.h"
#include "lvgl.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_cache.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ui";

/* Display geometry */
#define LCD_W           800
#define LCD_H           480
#define BUF_LINES       48          /* render buffer height (rows per chunk) */
#define PX_BYTES        2           /* RGB565 = 2 bytes/pixel */
#define BUF_SIZE        (LCD_W * BUF_LINES * PX_BYTES)

/* Cached LCD frame buffer address (PSRAM) */
static uint16_t *g_fb = NULL;

/* LVGL timer handle (keep alive for the timer's lifetime) */
static esp_timer_handle_t g_tick_timer = NULL;

/* ────────── LVGL display flush callback (PARTIAL mode) ────────── */
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if (g_fb == NULL) {
        lv_display_flush_ready(disp);
        return;
    }

    uint16_t *src = (uint16_t *)px_map;

    /* Copy each row from LVGL buffer to LCD frame buffer */
    for (int y = area->y1; y <= area->y2; y++) {
        int dst_off = y * LCD_W + area->x1;
        int line_w  = area->x2 - area->x1 + 1;
        memcpy(&g_fb[dst_off], src, line_w * sizeof(uint16_t));
        src += line_w;
    }

    /* Flush CPU cache → PSRAM so LCD DMA sees the updated pixels */
    void *flush_start = &g_fb[area->y1 * LCD_W];
    size_t flush_size = (area->y2 - area->y1 + 1) * LCD_W * sizeof(uint16_t);
    esp_cache_msync(flush_start, flush_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    lv_display_flush_ready(disp);
}

/* ────────── LVGL touch input callback ────────── */
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

/* ────────── 5ms LVGL tick timer callback ────────── */
static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(5);
}

/* ────────── Public: initialize LVGL ────────── */
void ui_init(esp_lcd_panel_handle_t panel)
{
    /* 1. Init LVGL core */
    ESP_LOGI(TAG, "lv_init...");
    lv_init();

    /* 2. Create display object, 800x480, RGB565 */
    ESP_LOGI(TAG, "Creating display 800x480...");
    lv_display_t *disp = lv_display_create(LCD_W, LCD_H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    ESP_LOGI(TAG, "Color format: %d (expect 16 = RGB565)",
             (int)lv_display_get_color_format(disp));

    /* 3. Allocate two PARTIAL render buffers in PSRAM */
    ESP_LOGI(TAG, "Allocating PSRAM render buffers (%d bytes x2)...", BUF_SIZE);
    void *buf1 = malloc(BUF_SIZE);
    void *buf2 = malloc(BUF_SIZE);
    if (buf1 == NULL || buf2 == NULL) {
        ESP_LOGE(TAG, "Buffer allocation failed! Check PSRAM is enabled.");
        return;
    }
    lv_display_set_buffers(disp, buf1, buf2, BUF_SIZE,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* 4. Cache LCD frame buffer address */
    g_fb = (uint16_t *)lcd_get_fb(panel);
    if (g_fb == NULL) {
        ESP_LOGE(TAG, "Failed to get LCD frame buffer!");
        return;
    }
    ESP_LOGI(TAG, "LCD frame buffer @ %p (PSRAM)", (void *)g_fb);

    /* 5. Register flush callback */
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    /* 6. Register touch input device */
    ESP_LOGI(TAG, "Registering touch input...");
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_read_cb);

    /* 7. Start 5ms periodic tick timer for LVGL */
    ESP_LOGI(TAG, "Starting 5ms tick timer...");
    const esp_timer_create_args_t timer_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick"
    };
    esp_timer_create(&timer_args, &g_tick_timer);
    esp_timer_start_periodic(g_tick_timer, 5000);  /* 5000 us = 5 ms */

    ESP_LOGI(TAG, "LVGL initialized (PARTIAL mode, %d-row buffers)", BUF_LINES);
}

/* ────────── Public: create clock UI ────────── */
lv_obj_t *ui_create_clock(void)
{
    /* Get the active screen and set white background */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Create a large centered label for the time */
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "--:--:--");
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);

    /* Center the label on screen */
    lv_obj_center(label);

    ESP_LOGI(TAG, "Clock UI created (white background, black text centered)");
    return label;
}

/* ────────── Public: update time label ────────── */
void ui_update_time(lv_obj_t *label, const char *time_str)
{
    if (label == NULL || time_str == NULL) return;
    lv_label_set_text(label, time_str);
}
