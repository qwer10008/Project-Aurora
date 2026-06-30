# Project Aurora — 智能桌面闹钟 项目上下文

## 项目简介
一台自己能造、能改、能修到毕业的智能闹钟。帮助用户进行一天的学习规划。
当前阶段：Layer 1 MVP（显示时间 + 设闹钟 + 到点响）。

## 硬件平台

| 组件 | 型号 | 接口 |
|------|------|------|
| 主控 | ESP32-S3-DevKitC-1 (N16R8) | - |
| 屏幕 | 正点原子 4.3" IPS 800×480 电容触摸 | ST7262 RGB + GT911 I2C |
| 时钟 | DS3231M 高精度 RTC | I2C (SDA=GPIO41, SCL=GPIO42) |
| 音频 | MAX98357A I2S 功放 + 30mm 3W 喇叭 | I2S (GPIO4/5/7) |
| 传感器 | BH1750 光照 + SHT30 温湿度 + INMP441 麦克风 | 待接入 |

## 技术栈
- ESP-IDF v5.5（C 语言，CMake 构建）
- ESP32-S3 目标芯片（Flash 16MB, PSRAM Octal 80MHz）
- LVGL v9（GUI 框架，待集成）
- 驱动层：ESP-IDF 原生 I2C/I2S/RGB LCD 驱动 API

## 项目结构（组件化架构）

```
main/               ← 主程序，只负责初始化和调度循环
components/         ← 每个硬件模块一个文件夹
  drv_lcd/          ← 屏驱动：ST7262 RGB + 背光
  drv_touch/        ← 触摸驱动：GT911 I2C
  drv_rtc/          ← 时钟驱动：DS3231M I2C
  drv_audio/        ← 音频驱动：MAX98357A I2S
  drv_storage/      ← 存储驱动：NVS Flash 存取
```

**架构规则**：
- 每个组件 = `xxx.h`（公开接口）+ `xxx.c`（实现细节）
- main.c 只调用公共接口函数，不直接操作寄存器
- 组件之间解耦，一个组件不直接 include 另一个组件的 .h（除非有明确的依赖关系）
- 所有 .c 文件不超过 200 行

## 编码约定
1. 硬件参数用 `#define` 宏定义在 .c 文件顶部，不写进 .h
2. 公开接口函数命名：`模块名_动作()`，如 `ds3231_read_time()`
3. 非显而易见的逻辑必须加"为什么"注释（参见 main.c 中的注释风格）
4. 不引入项目中尚未使用的设计模式或抽象层
5. 不提前为未来需求设计
6. 所有外设操作必须交叉对照数据手册验证

## AI 协作规则
1. 每次生成代码后，解释关键逻辑
2. 每次修改前先说明要改哪些文件，为什么
3. 每次只修改一个模块
4. 每完成一个功能模块，更新 MODULES.md
5. 优先使用"AI 导航员"模式——用户写代码，AI 审查和提建议
6. 所有涉及硬件寄存器/I2C地址的代码，必须标注数据手册出处

## 构建和烧录

```bash
idf.py set-target esp32s3
idf.py menuconfig   # Flash: 16MB, PSRAM: Octal 80MHz
idf.py build
idf.py -p COM3 flash monitor
```

## 项目入口
- 主程序：main/main.c → app_main()
- CMake 入口：CMakeLists.txt（顶层）→ main/CMakeLists.txt（组件依赖声明）
