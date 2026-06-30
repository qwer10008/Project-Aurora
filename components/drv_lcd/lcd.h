#pragma once
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"

// 初始化 LCD（RGB 面板），返回面板句柄。背光暂不点亮。
esp_lcd_panel_handle_t lcd_init(void);

// 获取帧缓冲指针（RGB565 格式，800x480）
void *lcd_get_fb(esp_lcd_panel_handle_t panel);

// 点亮背光——在 LVGL 初始化完成后调用，确保第一帧画面已就绪
void lcd_backlight_on(void);
