/*
 * Project Aurora
 * Key fix: LVGL task starts BEFORE backlight on
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "storage.h"
#include "ds3231.h"
#include "audio.h"
#include "touch.h"
#include "lcd.h"
#include "ui.h"
#include "lvgl.h"

static const char *TAG = "aurora";
static char g_time_str[16];

void app_main(void)
{
    ESP_LOGI(TAG, "=== Project Aurora ===");
    ESP_LOGI(TAG, "[1] storage"); storage_init();
    ESP_LOGI(TAG, "[2] rtc");    ds3231_init();
    ESP_LOGI(TAG, "[3] audio");  audio_init();
    ESP_LOGI(TAG, "[4] touch");  touch_init();
    ESP_LOGI(TAG, "[5] lcd...");
    esp_lcd_panel_handle_t panel = lcd_init();
    if (panel == NULL) { ESP_LOGE(TAG, "LCD fail"); return; }
    ESP_LOGI(TAG, "[6] ui_init...");
    ui_init(panel);
    lv_obj_t *time_label = ui_create_clock();
    if (time_label == NULL) { ESP_LOGE(TAG, "UI fail"); return; }
    rtc_time_t now; memset(&now, 0, sizeof(now));
    if (ds3231_read_time(&now)) {
        snprintf(g_time_str, sizeof(g_time_str), "%02d:%02d:%02d", now.hour, now.min, now.sec);
        ui_update_time(time_label, g_time_str);
        ESP_LOGI(TAG, "time=%s", g_time_str);
    } else {
        ui_update_time(time_label, "--:--:--");
        ESP_LOGW(TAG, "RTC fail");
    }
    ESP_LOGI(TAG, "Starting LVGL task (backlight OFF)...");
    ui_start_task();
    vTaskDelay(pdMS_TO_TICKS(200));
    lcd_backlight_on();
    ESP_LOGI(TAG, "===== Main loop =====");
    int rtc_ok = 0, rtc_fail = 0;
    int64_t last_rtc = esp_timer_get_time();
    while (1) {
        int64_t tn = esp_timer_get_time();
        if (tn - last_rtc >= 500000) {
            last_rtc = tn;
            if (ds3231_read_time(&now)) { rtc_ok++;
                snprintf(g_time_str, sizeof(g_time_str), "%02d:%02d:%02d", now.hour, now.min, now.sec);
                ui_update_time(time_label, g_time_str);
            } else { rtc_fail++; }
            if ((rtc_ok + rtc_fail) % 20 == 0)
                ESP_LOGI(TAG, "RTC OK:%d FAIL:%d", rtc_ok, rtc_fail);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}