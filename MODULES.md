# Project Aurora 模块清单

## main/ — 主程序（调度层）

### main.c
**职责**：硬件初始化 + 主循环调度（RTC 读取 → LVGL 时间显示更新）。

**公开接口**：`void app_main(void)` — ESP-IDF 程序入口。

**依赖的组件**（按初始化顺序）：
1. drv_storage — `storage_init()`（NVS 最先）
2. drv_rtc — `ds3231_init()`（顺带初始化 I2C 总线 GPIO41/42）
3. drv_audio — `audio_init()`（独立 I2S）
4. drv_touch — `touch_init()`（复用 I2C，必须在 LCD 之前）
5. drv_lcd — `lcd_init()`（RGB 面板初始化）
6. drv_ui — `ui_init()`（LVGL 初始化，依赖 LCD 帧缓冲）

**当前状态**：Layer 1 MVP — 屏幕显示白色背景 + 黑色时间数字，RTC 驱动实时更新。

---

## components/drv_lcd/ — LCD 屏幕驱动

| 文件 | 说明 |
|------|------|
| lcd.h | 公开接口：`lcd_init()`, `lcd_get_fb()`, `lcd_backlight_on()` |
| lcd.c | 实现：RGB 面板初始化 + 帧缓冲管理 + 背光控制 |
| CMakeLists.txt | 组件注册，依赖 esp_lcd |

**硬件**：正点原子 4.3" IPS 800×480，ST7262 RGB 接口。

**公开接口**：
```c
esp_lcd_panel_handle_t lcd_init(void);             // 初始化 RGB 面板，返回句柄，填充白屏
void *lcd_get_fb(esp_lcd_panel_handle_t panel);    // 获取帧缓冲指针（RGB565, 800×480）
void lcd_backlight_on(void);                        // 点亮背光（在 LVGL 初始化后调用）
```

**依赖**：ESP-IDF `esp_lcd` 组件（`esp_lcd_panel_rgb.h`）。

---

## components/drv_touch/ — 触摸驱动

| 文件 | 说明 |
|------|------|
| touch.h | 公开接口：`touch_init()`, `touch_read()` |
| touch.c | 实现：GT911 I2C 通信 + 坐标解析 |
| CMakeLists.txt | 组件注册 |

**硬件**：GT911 电容触摸控制器，I2C 地址 0x5D（或 0x14）。

**公开接口**：
```c
bool touch_init(void);                    // 初始化 GT911（I2C 必须已初始化），成功返回 true
bool touch_read(uint16_t *x, uint16_t *y); // 读取触摸点坐标，有触摸返回 true
```

**注意**：必须在 LCD 之前初始化，因为 LCD 亮起后 I2C 总线上会有噪声干扰触摸芯片通信。

---

## components/drv_rtc/ — 实时时钟驱动

| 文件 | 说明 |
|------|------|
| ds3231.h | 公开接口 + `rtc_time_t` 结构体定义（**改名避免与 ESP-IDF 内置 rtc.h 冲突**） |
| rtc.c | 实现：I2C 读写 + BCD 编解码 + 数据校验 |
| CMakeLists.txt | 组件注册 |

**硬件**：DS3231M，I2C 地址 0x68，SDA=GPIO41, SCL=GPIO42，速率 10kHz。

**公开接口**：
```c
typedef struct {
    uint8_t sec, min, hour;
    uint8_t day, month, year;
} rtc_time_t;

void ds3231_init(void);                          // 初始化 I2C 总线（GPIO41+42，上拉已启用）
bool ds3231_read_time(rtc_time_t *t);            // 读时间，失败返回 false（带 BCD 合法性校验）
void ds3231_write_time(const rtc_time_t *t);     // 写时间（设置时钟用）
```

**依赖**：ESP-IDF `driver/i2c`（`i2c.h`）。

**注意**：头文件名为 `ds3231.h`（非 `rtc.h`），因 ESP-IDF 内置了冲突的 `rtc.h`。

---

## components/drv_audio/ — 音频驱动

| 文件 | 说明 |
|------|------|
| audio.h | 公开接口：`audio_init()`, `audio_play_tone()` |
| audio.c | 实现：I2S 初始化 + 正弦波生成（PCM 样本计算） |
| CMakeLists.txt | 组件注册 |

**硬件**：MAX98357A I2S 功放，GPIO4(BCLK)/GPIO5(LRCK)/GPIO7(DIN)，30mm 3W 喇叭。

**公开接口**：
```c
void audio_init(void);                            // 初始化 I2S 输出通道
void audio_play_tone(int freq_hz, int duration_ms); // 播放正弦波，阻塞式
```

**依赖**：ESP-IDF `driver/i2s`。

---

## components/drv_storage/ — NVS 存储驱动

| 文件 | 说明 |
|------|------|
| storage.h | 公开接口：`storage_init()`, `storage_save_alarm()`, `storage_load_alarm()` |
| storage.c | 实现：NVS 命名空间读写 |
| CMakeLists.txt | 组件注册 |

**硬件**：ESP32-S3 内部 Flash（通过 NVS 分区）。

**公开接口**：
```c
void storage_init(void);                          // 初始化 NVS 分区
void storage_save_alarm(uint16_t minutes);        // 保存闹钟时间（0-1439 分钟）
uint16_t storage_load_alarm(void);                // 读取闹钟时间，默认 420（7:00）
```

**依赖**：ESP-IDF `nvs_flash`。

---

## components/drv_ui/ — LVGL 图形库对接层

| 文件 | 说明 |
|------|------|
| ui.h | 公开接口：`ui_init()`, `ui_create_clock()`, `ui_update_time()` |
| ui.c | 实现：LVGL 初始化 + PARTIAL 模式 flush 回调 + 触摸输入 + 滴答定时器 |
| CMakeLists.txt | 组件注册，依赖 lvgl、drv_lcd、drv_touch、esp_timer、esp_mm |

**硬件**：无（纯软件层，对接 drv_lcd 的帧缓冲和 drv_touch 的触摸数据）。

**公开接口**：
```c
void ui_init(esp_lcd_panel_handle_t panel);          // 初始化 LVGL，接管 LCD 帧缓冲 + 注册触摸输入
lv_obj_t *ui_create_clock(void);                     // 创建时钟界面（白色背景 + 黑色时间居中）
void ui_update_time(lv_obj_t *label, const char *time_str); // 更新时间显示
```

**依赖**：
- `lvgl/lvgl` (v9.5.0，通过 IDF 组件管理器 `idf.py add-dependency` 安装)
- drv_lcd — `lcd_get_fb()` 获取帧缓冲给 LVGL 当画布
- drv_touch — `touch_read()` 喂给 LVGL 输入系统
- `esp_timer` — 5ms 滴答定时器驱动 LVGL 内部时钟

**内部细节**：
- PARTIAL 渲染模式：LVGL 用 PSRAM 中的双缓冲渲染，flush 回调逐行拷贝到 LCD 帧缓冲
- 单帧缓冲（num_fbs=1），PSRAM 分配 ~77KB（800×48×2）的双缓冲
- flush 后调用 `esp_cache_msync()` 刷新缓存，确保 LCD DMA 看到新数据
- 5ms 硬件定时器调用 `lv_tick_inc(5)` 驱动 LVGL 动画和定时器
- 触摸数据通过 `lv_indev_set_read_cb()` 注册到 LVGL 输入系统
- 字体使用 Montserrat 48（大数字显示时间）

---

## 组件依赖关系图

```
main.c
  ├── drv_storage (NVS 初始化)
  ├── drv_rtc     (I2C 总线 + DS3231M 操作)
  ├── drv_audio   (I2S 音频输出)
  ├── drv_touch   (GT911 触摸 → 依赖 drv_rtc 初始化的 I2C)
  ├── drv_lcd     (RGB 面板 + 背光)
  └── drv_ui      (LVGL 图形库 → 依赖 drv_lcd + drv_touch + lvgl)
```

**关键约束**：
1. drv_rtc 必须先于 drv_touch 初始化（I2C 总线由 drv_rtc 安装）
2. drv_touch 必须先于 drv_lcd 初始化（LCD 亮起后 I2C 有噪声）
3. `lcd_backlight_on()` 在 LVGL 创建完第一帧后才调用（避免显示垃圾数据）
4. 各驱动组件之间不互相依赖，只通过 main.c 调度
5. 新 I2C 设备不得调用 `i2c_driver_install()`，必须复用 drv_rtc 的 I2C_NUM_0

---

## MVP 路线图

| Layer | 内容 | 状态 |
|-------|------|------|
| Layer 0 | ESP-IDF 环境搭建 · 串口 Hello World | 完成 |
| Layer 1 | 显示时间 + 设闹钟 + 到点响 | **当前**（时间显示已完成） |
| Layer 2 | 传感器联动：光照自动调亮度 | 待开发 |
| Layer 3 | Wi-Fi + MQTT 联网校时 | 待开发 |
| Layer 4 | 语音控制（INMP441 + ESP-SR） | 待开发 |
| Layer 5 | 手机 App + 邮件通知 | 待开发 |
| Layer 6 | 自定义 PCB + 3D 打印外壳 | 待开发 |

---

## 关键文件索引

| 文件 | 用途 |
|------|------|
| `main/CMakeLists.txt` | 主组件依赖声明 |
| `main/lv_conf.h` | LVGL 配置（RGB565、stdlib malloc、最小功能集） |
| `main/idf_component.yml` | 组件管理器清单（lvgl 依赖） |
| `dependencies.lock` | 组件锁定版本 |
| `managed_components/lvgl__lvgl/` | 自动下载的 LVGL 9.5.0 |
| `install_lvgl.bat` | LVGL 安装脚本（保留备查） |
