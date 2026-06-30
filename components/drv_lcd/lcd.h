#pragma once
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"

// 初始化 LCD（背光 + RGB 面板），返回面板句柄
esp_lcd_panel_handle_t lcd_init(void);

// 获取帧缓冲指针（RGB565 格式，800x480）
void *lcd_get_fb(esp_lcd_panel_handle_t panel);
