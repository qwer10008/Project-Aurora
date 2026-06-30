#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t sec, min, hour;
    uint8_t day, month, year;
} rtc_time_t;

void ds3231_init(void);
bool ds3231_read_time(rtc_time_t *t);
void ds3231_write_time(const rtc_time_t *t);
