# Servo Limit Detect — 舵机堵转限位标定

基于 STM32F407 的 **27 路舵机** 堵转电流检测与限位角度自动标定系统。

## 硬件平台

| 项目 | 型号 / 参数 |
|------|-------------|
| MCU | STM32F407VETx, 168 MHz |
| 舵机驱动 | PCA9685 (I²C) |
| 电流检测 | INA229 (SPI) |
| LCD | 2.8" ILI9341 SPI 240×320 |
| 数码管 | TM1637 4 位 |
| 调试输出 | UART1 (PA9, 115200 8N1) / SEGGER RTT 可选 |
| 按键 | KEY1 (PA0) 继电器, KEY2 (PA1) 启动标定, KEY3 (PA4) 预留 |

## 软件架构

```
STM32F407 (ARMCLANG V6.23)
├── FreeRTOS (CMSIS-RTOS v2)
├── HAL 驱动层
├── BSP 板级支持包
│   ├── servo / servo_actuator    PCA9685 舵机驱动
│   ├── ina229 / current           INA229 电流采集
│   ├── servo_calib                标定状态机 + 数据表
│   ├── app_servo                  标定任务 (App_sweep_task)
│   ├── app_lcd                    LCD 显示任务
│   ├── keys                       按键扫描 (消抖)
│   ├── relay / app_relay          继电器控制
│   ├── bsp_log                    日志输出 (RTT / UART 双模)
│   ├── bsp_LCD_ILI9341            ILI9341 驱动
│   └── sweep                      扫频功能
└── SEGGER_RTT                     RTT 调试组件
```

## 标定流程

```
按 KEY2 启动
  │
  ├─ TIM7 计时器复位 (10Hz CNT)
  │
  └─ 对每个舵机 (A0 ~ A26):
      ├─ CALIB_IDLE        归中 → 等待 500ms
      ├─ CALIB_SEARCH_UP   以 2° 歩进向外搜索
      │     终止条件: I ≥ 0.6A 或 角度 ≥ 270°
      │     → 记录 upper_limit_deg
      ├─ CALIB_TRANSITION  归中 → 等待 500ms
      ├─ CALIB_SEARCH_DOWN 以 2° 歩进向内搜索
      │     终止条件: I ≥ 0.6A 或 角度 ≤ 0°
      │     → 记录 lower_limit_deg
      └─ CALIB_NEXT_SERVO  归中 → 下一个舵机
  │
  └─ CALIB_DONE
      ├─ UART 输出 servo_limits[] 完整报告
      └─ UART 输出总耗时
```

## 关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| CALIB_HARD_MIN | 0° | 机械下限 |
| CALIB_HARD_MAX | 270° | 机械上限 |
| 搜索步长 | 2° | 上下搜索统一 |
| 堵转电流 | 0.6 A | INA229 实测阈值 |
| TIM7 时钟 | 10 Hz | PSC=139, ARR=59999 |

## 输出切换

修改 [bsp_log.h](BSP/Inc/bsp_log.h) 第 16 行：

```c
#define LOG_USE_UART    1   // 1 = UART1,  0 = SEGGER RTT
```

## 目录结构

```
Servo_limit_detect/
├── BSP/                        # 板级支持包 (源码)
│   ├── Inc/                    # 头文件
│   └── Src/                    # 源文件
├── Core/                       # STM32CubeMX 生成
│   ├── Inc/                    # HAL 配置 / FreeRTOSConfig
│   └── Src/                    # main, 中断, 外设初始化
├── Drivers/                    # HAL 库 + CMSIS
├── MDK-ARM/                    # Keil MDK 工程
├── Middlewares/                # FreeRTOS
└── SEGGER_RTT/                 # RTT 源码
```

## 构建

Keil MDK-ARM 打开 `MDK-ARM/Servo_limit_detect.uvprojx`，ARMCLANG V6.23 编译。

> **注意**：需勾选 **Use MicroLIB**（Target → Code Generation）。