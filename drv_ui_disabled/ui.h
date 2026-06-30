#pragma once
#include "esp_lcd_panel_rgb.h"

// 初始化 LVGL：在 LCD 帧缓冲上创建显示、注册触摸输入、启动滴答定时器
// 必须在 lcd_init() 之后、创建任何 LVGL 控件之前调用
void ui_init(esp_lcd_panel_handle_t panel);
