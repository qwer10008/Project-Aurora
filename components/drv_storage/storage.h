#pragma once
#include <stdint.h>

// 初始化 NVS
void storage_init(void);

// 存取闹钟时间（0-1439 分钟，例如 7:00 = 420）
void storage_save_alarm(uint16_t minutes);
uint16_t storage_load_alarm(void);
