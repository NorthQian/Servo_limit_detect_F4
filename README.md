# Servo Limit Detect — 舵机堵转限位标定

基于 STM32F407 的 **27 路舵机**堵转电流检测与限位角度自动标定系统，配套 PyQt5 上位机串口工具。

---

## 功能概述

### 1. 舵机限位标定（核心功能）

通过 INA229 高精度电流传感器实时监测舵机工作电流，自动扫描每个舵机的上下机械限位角度：

- **上限度搜索**：舵机从中心位置以 2° 步进向外旋转，当电流超过堵转阈值（0.6A）或到达机械上限（270°）时停止，记录 `upper_limit_deg`
- **下限度搜索**：舵机返回中心后以 2° 步进向内旋转，当电流超过堵转阈值或到达机械下限（0°）时停止，记录 `lower_limit_deg`
- **非阻塞状态机**：基于 FreeRTOS 任务调度，`CALIB_IDLE` → `CALIB_SEARCH_UP` → `CALIB_TRANSITION` → `CALIB_SEARCH_DOWN` → `CALIB_NEXT_SERVO` → `CALIB_DONE`
- **硬件配置切换**：支持 `HW_CONFIG_HIGH`（全 27 路）和 `HW_CONFIG_LOW`（跳过眉毛/脸颊/嘴唇等 10 路，仅标定 17 路）
- **标定结果**：通过 UART1 输出完整的 `servo_limits[]` 表格报告（含每路通道号、中心角、上限、下限及总耗时）

### 2. 舵机扫频扫描

独立于标定之外的扫频功能，用于验证限位数据或演示舵机运动范围：

- 每路舵机按 **中心 → 上限度 → 下限度 → 中心** 的路径来回扫描
- 非阻塞状态机实现（`SWEEP_TO_UPPER` → `SWEEP_TO_LOWER` → `SWEEP_TO_CENTER` → `SWEEP_NEXT`），不阻塞其他 FreeRTOS 任务
- 步长、间隔时间、运动时间均可配置

### 3. 上位机串口工具（[Tools/SerialTool/](Tools/SerialTool/)）

基于 PyQt5 的 Windows 串口接收与 YAML 导出工具：

- **串口连接**：支持常用波特率（9600 ~ 921600），自动枚举可用串口
- **数据解析**：自动识别 STM32 输出的 `servo_limits[]` 标定报告，解析出每路舵机的通道号、中心角、上下限度
- **YAML 模板合并**：加载参考 `servo_config.yaml`，将标定结果按 `channel_idx` 自动匹配写入对应舵机的 `start_deg` / `end_deg` / `temp_deg`，保持原始数据的大小方向关系
- **一键导出**：输出带时间戳的 YAML 配置文件，可直接用于后续舵机控制

### 4. 其他外设功能

- **LCD 显示**：2.8" ILI9341 SPI 触摸屏，实时显示标定进度和状态
- **数码管**：TM1637 4 位数码管
- **继电器控制**：KEY1 触发，控制外部电源等设备
- **按键输入**：KEY1 继电器、KEY2 启动标定、KEY3 预留
- **双模日志输出**：编译时可通过宏切换 UART1 或 SEGGER RTT

---

## 硬件平台

| 项目 | 型号 / 参数 |
|------|-------------|
| MCU | STM32F407VETx, 168 MHz |
| 舵机驱动 | PCA9685 (I²C, 27 路) |
| 电流检测 | INA229 (SPI, 20 位 Δ-Σ ADC) |
| LCD | 2.8" ILI9341 SPI 240×320 |
| 数码管 | TM1637 4 位 |
| 调试输出 | UART1 (PA9, 115200 8N1) / SEGGER RTT |
| 按键 | KEY1 (PA0) 继电器, KEY2 (PA1) 启动标定, KEY3 (PA4) 预留 |
| LED | LED_BLUE (PE5) 标定状态指示 |

---

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
│   ├── sweep                      扫频扫描模块
│   ├── app_lcd                    LCD 显示任务
│   ├── keys                       按键扫描（消抖）
│   ├── relay / app_relay          继电器控制
│   ├── bsp_log                    日志输出（RTT / UART 双模）
│   └── bsp_LCD_ILI9341            ILI9341 驱动
├── SEGGER_RTT                     RTT 调试组件
└── Tools/
    └── SerialTool/                PyQt5 上位机串口工具
        ├── main.py                主程序
        └── requirements.txt       依赖清单
```

---

## FreeRTOS 任务一览

| 任务 | 功能 | 优先级 |
|------|------|--------|
| `defaultTask` | 启动任务，创建其他任务后自删除 | osPriorityNormal |
| `App_sweep_task` | 标定扫描主任务（状态机循环） | osPriorityNormal |
| `App_sniff_task` | 舵机嗅探/扫频任务 | osPriorityNormal |
| `App_Relay_task` | 继电器控制（KEY1 触发） | osPriorityNormal |

---

## 标定流程

```
按 KEY2 启动
  │
  ├─ LED_BLUE 亮，TIM7 计时器启动
  │
  └─ 对每个舵机 (idx 0 ~ 26):
      ├─ CALIB_IDLE          跳过检查 (HW_CONFIG_LOW 时跳过眉毛/脸颊/嘴唇)
      │                      归中 → 等待 500ms
      ├─ CALIB_SEARCH_UP     以 2° 歩进向外搜索
      │                      终止条件: I ≥ 0.6A 或 角度 ≥ 270°
      │                      → 记录 upper_limit_deg
      ├─ CALIB_TRANSITION    归中 → 等待 500ms
      ├─ CALIB_SEARCH_DOWN   以 2° 歩进向内搜索
      │                      终止条件: I ≥ 0.6A 或 角度 ≤ 0°
      │                      → 记录 lower_limit_deg
      └─ CALIB_NEXT_SERVO    归中 → idx++，下一个舵机
  │
  └─ CALIB_DONE
      ├─ UART 输出 servo_limits[] 完整报告
      └─ UART 输出总耗时 (秒)
```

---

## 舵机配置

27 路舵机功能映射（对应人形机器人面部表情控制）：

| Idx | CH | Center | 名称 | 说明 |
|-----|----|--------|------|------|
| 0 | 0 | 135° | L_eyebrow_head_ver | 左眉头垂直 |
| 1 | 1 | 135° | R_eyebrow_head_ver | 右眉头垂直 |
| 2 | 2 | 135° | L_eyebrow_head_hor | 左眉头水平 |
| 3 | 3 | 135° | R_eyebrow_head_hor | 右眉头水平 |
| 4 | 4 | 135° | L_eyebrow_tail | 左眉尾 |
| 5 | 5 | 135° | R_eyebrow_tail | 右眉尾 |
| 6 | 6 | 135° | L_eyebrow_hor | 左眉水平 |
| 7 | 7 | 135° | R_eyebrow_hor | 右眉水平 |
| 8 | 8 | 140° | L_upper_eyelid | 左上眼睑 |
| 9 | 9 | 125° | R_upper_eyelid | 右上眼睑 |
| 10 | 10 | 135° | L_vertical_eyeball | 左眼球垂直 |
| 11 | 11 | 135° | R_vertical_eyeball | 右眼球垂直 |
| 12 | 12 | 135° | L_horizontal_eyeball | 左眼球水平 |
| 13 | 13 | 135° | R_horizontal_eyeball | 右眼球水平 |
| 14 | 14 | 135° | R_cheek | 右脸颊 |
| 15 | 15 | 135° | L_cheek | 左脸颊 |
| 16 | 16 | 147° | mid_upper_lip_ver | 中上唇垂直 |
| 17 | 17 | 190° | mid_upper_lip_hor | 中上唇水平 |
| 18 | 18 | 165° | mid_lower_lip_hor | 中下唇水平 |
| 19 | 19 | 155° | mid_lower_lip_ver | 中下唇垂直 |
| 20 | 20 | 115° | upper_lip | 上唇 |
| 21 | 21 | 105° | lower_lip | 下唇 |
| 22 | 22 | 135° | R_upper_mouth | 右上嘴角 |
| 23 | 23 | 135° | L_lower_mouth | 左下嘴角 |
| 24 | 24 | 130° | L_upper_mouth | 左上嘴角 |
| 25 | 25 | 135° | R_lower_mouth | 右下嘴角 |
| 26 | 26 | 95° | chin_vertical | 下巴垂直 |

> **HW_CONFIG_LOW** 模式下，以下舵机被跳过（不参与标定，限位直接设为 0° ~ 270°）：
> 眉毛水平 (idx 2, 3, 6, 7)、眉尾 (idx 4, 5)、脸颊 (idx 14, 15)、嘴唇 (idx 20, 21)

---

## 关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| `CALIB_HARD_MIN` | 0° | 机械下限 |
| `CALIB_HARD_MAX` | 270° | 机械上限 |
| `CALIB_STEP_UP_DEG` | 2° | 上限搜索步长 |
| `CALIB_STEP_DOWN_DEG` | 5° | 下限搜索步长（可独立配置） |
| 堵转电流阈值 | 0.8 A (默认) | INA229 实测，可逐路配置 |
| 堵转去抖时间 | 500 ms | 电流须持续超阈值方判定堵转 |
| 消隐时间 | 100 ms | 动作后忽略尖峰电流的窗口 |
| TIM7 时钟 | 10 Hz | 标定计时器 |
| 扫频步长 | 45° (默认) | Sweep_Init 可配置 |
| 扫频间隔 | 500 ms (默认) | Sweep_Init 可配置 |

---

## 上位机使用说明

### 环境安装

```bash
cd Tools/SerialTool
pip install -r requirements.txt
```

依赖：`PyQt5 >= 5.15`、`PyYAML >= 6.0`

### 启动

```bash
python main.py
```

### 操作步骤

1. **加载模板**：点击"加载模板"，选择参考 `servo_config.yaml`（需包含 `SERVO_INFO` 节点，每路舵机含 `channel_idx`）
2. **连接串口**：选择 STM32 对应的 COM 口和波特率（默认 115200），点击"打开串口"
3. **启动标定**：按下 STM32 的 KEY2，等待标定完成
4. **自动解析**：工具自动识别串口输出的 `servo_limits[]` 表格，状态栏提示"已解析 N 个舵机数据"
5. **导出 YAML**：点击"导出 YAML"，选择保存路径，工具将标定结果合并写入模板并添加时间戳

### 串口数据格式

STM32 标定完成后输出的格式（上位机按此格式解析）：

```
============================================================
  Servo Limit Sweep Results (servo_limits[])
------------------------------------------------------------
  Idx  CH  Center  Lower  Upper
------------------------------------------------------------
  0     0   135     90     180
  1     1   135     85     185
  ...
============================================================
  Total: 27 servos
============================================================
Sweep total time: 45s
```

### 导出 YAML 结构

导出的 YAML 文件包含 `HW_INFO`（校准日期、耗时）和 `SERVO_INFO`（每路舵机的完整限位数据），可直接用于舵机控制系统。

---

## 输出切换

修改 [bsp_log.h](BSP/Inc/bsp_log.h) 第 16 行：

```c
#define LOG_USE_UART    1   // 1 = UART1,  0 = SEGGER RTT
```

---

## 目录结构

```
Servo_limit_detect/
├── BSP/                            # 板级支持包（源码）
│   ├── Inc/                        # 头文件
│   │   ├── servo.h                 # 舵机限位数据结构
│   │   ├── servo_calib.h           # 标定状态机
│   │   ├── sweep.h                 # 扫频模块
│   │   ├── servo_actuator.h        # PCA9685 驱动
│   │   ├── current.h / ina229.h    # INA229 电流采集
│   │   ├── app_servo.h             # 标定任务接口
│   │   ├── bsp_log.h               # 日志输出接口
│   │   └── keys.h / relay.h / ...  # 其他外设
│   └── Src/                        # 源文件
│       ├── servo_calib.c           # 标定实现 + 数据表
│       ├── sweep.c                 # 扫频实现
│       ├── app_servo.c             # 标定 FreeRTOS 任务
│       └── ...
├── Core/                           # STM32CubeMX 生成
│   ├── Inc/                        # HAL 配置 / FreeRTOSConfig
│   └── Src/                        # main, 中断, 外设初始化, freertos
├── Drivers/                        # HAL 库 + CMSIS
├── MDK-ARM/                        # Keil MDK 工程
├── Middlewares/                    # FreeRTOS
├── SEGGER_RTT/                     # RTT 源码
└── Tools/
    └── SerialTool/                 # 上位机串口工具
        ├── main.py                 # PyQt5 GUI 主程序
        └── requirements.txt        # Python 依赖
```

---

## 构建

Keil MDK-ARM 打开 `MDK-ARM/Servo_limit_detect.uvprojx`，ARMCLANG V6.23 编译。

> **注意**：需勾选 **Use MicroLIB**（Target → Code Generation）。
