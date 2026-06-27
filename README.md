# Project Aurora — 智能桌面闹钟

> 一台自己能造、能改、能修到毕业的智能闹钟。

![ESP32-S3](https://img.shields.io/badge/ESP32--S3-N16R8-blue)
![LVGL|48](https://img.shields.io/badge/LVGL-v9-green)
![ESP--IDF](https://img.shields.io/badge/ESP--IDF-v5.5-purple)
![Stage](https://img.shields.io/badge/Stage-Layer%201%20MVP-orange)

---

## 为什么要做这个

市面上并没有合适的智能闹钟，普通闹钟用了又觉得局限性很大。

我想要的是一台**智能计划**的闹钟——帮助我进行一天的学习。

目前是准高中生。把嵌入式开发、C 语言、PCB 设计一路学下去，最终造出自己的东西。

---

## 硬件

| 组件 | 型号 | 
|------|------|
| 主控 | ESP32-S3-DevKitC-1 (N16R8) |
| 屏幕 | 正点原子 4.3" IPS 800×480 电容触摸 |
| 时钟 | DS3231M I2C 高精度 RTC |
| 音频 | MAX98357A I2S 功放 + 30mm 3W 喇叭 |
| 传感器 | BH1750 光照 + SHT30 温湿度 + INMP441 麦克风 |

> [完整物料清单](链接到 BOM)

---

## 项目结构

```
aurora/
├── main/           # 主程序
├── components/
│   ├── lvgl/       # LVGL 图形库
│   ├── drv_lcd/    # ST7262 RGB 屏驱动
│   ├── drv_touch/  # GT911 触摸驱动
│   ├── drv_rtc/    # DS3231M 时钟驱动
│   ├── drv_audio/  # MAX98357A 音频驱动
│   └── ui/         # 界面逻辑
├── docs/           # 接线表、学习笔记
└── platformio.ini
```

---

## MVP 路线图

- [ ] **Layer 0** — ESP-IDF 环境搭建 · 串口 Hello World
- [ ] **Layer 1** — 显示时间 + 设闹钟 + 到点会响（当前）
- [ ] **Layer 2** — 传感器联动：光照自动调亮度
- [ ] **Layer 3** — Wi-Fi + MQTT 联网校时
- [ ] **Layer 4** — 语音控制（INMP441 + ESP-SR）
- [ ] **Layer 5** — 手机 App + 邮件通知
- [ ] **Layer 6** — 自定义 PCB + 3D 打印外壳

---

## 文件导航

| 要找一个 | 点这儿 |
|----------|--------|
| 接线怎么接 | [Layer 1 接线表](链接) |
| 为什么选这个芯片 | [硬件选型笔记](链接) |
| ESP-IDF 怎么配 | [menuconfig 配置记录](链接) |
| 踩过的坑 | [开发日志](链接) |

---

## 快速开始

```bash
git clone <repo-url>
cd aurora
idf.py set-target esp32s3
idf.py menuconfig   # Flash: 16MB, PSRAM: Octal 80MHz
idf.py build
idf.py -p COM3 flash monitor
```

---

## 许可

Apache License 2.0 — 暂时先用这个吧，我对许可证这一方面还不是很了解。

---

*Built by a junior high student who wanted a better alarm clock.*

