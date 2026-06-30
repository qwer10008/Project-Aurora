#include "storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "storage";
static nvs_handle_t handle = 0;
static bool initialized = false;

void storage_init(void)
{
    if (initialized) return;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(nvs_open("alarm", NVS_READWRITE, &handle));
    initialized = true;
    ESP_LOGI(TAG, "NVS 初始化完成");
}

void storage_save_alarm(uint16_t minutes)
{
    if (!initialized) return;
    nvs_set_u16(handle, "alarm_min", minutes);
    nvs_commit(handle);
    ESP_LOGI(TAG, "闹钟已保存: %u 分钟", minutes);
}

uint16_t storage_load_alarm(void)
{
    if (!initialized) return 420;  // 默认 7:00
    uint16_t val = 420;
    esp_err_t err = nvs_get_u16(handle, "alarm_min", &val);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "闹钟已读取: %u 分钟", val);
    }
    return val;
}
