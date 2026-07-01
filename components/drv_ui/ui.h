#pragma once
#include "esp_lcd_panel_rgb.h"
#include "lvgl.h"

/**
 * Initialize LVGL: bind to LCD frame buffer, register touch, start tick.
 * Creates the LVGL mutex but does NOT start rendering task yet.
 */
void ui_init(esp_lcd_panel_handle_t panel);

/**
 * Start the LVGL rendering task in a separate FreeRTOS thread.
 * Call this AFTER all UI objects have been created and initial state is set.
 */
void ui_start_task(void);

/**
 * Create clock UI: white background + centered black time label.
 * Must be called after ui_init(), before ui_start_task().
 */
lv_obj_t *ui_create_clock(void);

/**
 * Update the time label text (thread-safe, uses LVGL mutex).
 */
void ui_update_time(lv_obj_t *label, const char *time_str);
