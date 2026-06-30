#pragma once
#include <stdbool.h>
#include <stdint.h>

// 初始化 GT911 触摸（I2C 必须已初始化）
bool touch_init(void);

// 读取一个触摸点坐标，有触摸返回 true
bool touch_read(uint16_t *x, uint16_t *y);
