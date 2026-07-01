#pragma once
#include "esp_lcd_panel_rgb.h"
#include "lvgl.h"

/**
 * Initialize LVGL: bind to LCD frame buffer, register touch input, start tick timer.
 * Must be called after lcd_init() and before creating any LVGL objects.
 * @param panel LCD panel handle from lcd_init()
 */
void ui_init(esp_lcd_panel_handle_t panel);

/**
 * Create the clock display UI: white background, black time digits centered on screen.
 * Must be called after ui_init().
 * Returns the time label object so main.c can update its text.
 */
lv_obj_t *ui_create_clock(void);

/**
 * Update the time label text.
 * @param label the label object returned by ui_create_clock()
 * @param time_str null-terminated string like "14:30:05"
 */
void ui_update_time(lv_obj_t *label, const char *time_str);
