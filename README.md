# Servo Limit Detect — 舵机堵转限位标定

基于 STM32F407 的 **27 路舵机**堵转电流检测与限位角度自动标定系统，配套 PyQt5 上位机串口工具与 LVGL 触摸屏实时状态显示，另含机械底盘结构设计。

![LCD 显示](pic/lcd_disp.jpg)

---

## 功能概述

### 1. 舵机限位标定（核心功能）

通过 INA229 高精度电流传感器实时监测舵机工作电流，自动扫描每个舵机的上下机械限位角度：

- **上限度搜索**：舵机从中心位置以 2° 步进向外旋转，当电流超过堵转阈值（0.6A）或到达机械上限（270°）时停止，记录 `upper_limit_deg`
- **下限度搜索**：舵机返回中心后以 2° 步进向内旋转，当电流超过堵转阈值或到达机械下限（0°）时停止，记录 `lower_limit_deg`
- **非阻塞状态机**：基于 FreeRTOS 任务调度，`CALIB_IDLE` → `CALIB_SEARCH_UP` → `CALIB_TRANSITION` → `CALIB_SEARCH_DOWN` → `CALIB_NEXT_SERVO` → `CALIB_DONE`
- **硬件配置切换**：支持 `HW_CONFIG_HIGH`（全 27 路）和 `HW_CONFIG_LOW`（跳过眉毛/脸颊/嘴唇等 10 路，仅标定 17 路）
- **标定结果**：通过 UART1 输出完整的 `servo_limits[]` 表格报告（含每路通道号、中心角、上限、下限及总耗时）

### 2. 扫频扫描

独立于标定之外的扫频功能，用于验证限位数据或演示舵机运动范围：

- 每路舵机按 **中心 → 上限度 → 下限度 → 中心** 的路径来回扫描
- 非阻塞状态机实现（`SWEEP_TO_UPPER` → `SWEEP_TO_LOWER` → `SWEEP_TO_CENTER` → `SWEEP_NEXT`），不阻塞其他 FreeRTOS 任务
- 步长、间隔时间、运动时间均可配置

> 注：`BSP/Src/sweep.c` 提供独立的 `Sweep_Init/Start/Run` 模块，当前固件中实际扫频由 `app_servo.c` 的 `App_sweep_task` 状态机驱动，`sweep.c` 保留备用。

### 3. LVGL 触控显示

移植 **LVGL 8.3** 到 2.8" ILI9341，替代原先的直接绘制方式，提供实时状态仪表盘：

- **电流表盘**：`lv_meter` 指针式仪表（0 ~ 2000 mA），绿色（< 0.6A）/ 黄色（0.6 ~ 1A）/ 红色（1 ~ 2A）分区着色，指针带 300 ms 平滑动画
- **状态卡片**：当前扫频状态（IDLE / SEARCH_UP / TRANSITION / SEARCH_DOWN / NEXT_SERVO / DONE）同色圆点 + 文字
- **进度条**：已完成 / 总扫频舵机数（扣除跳过的舵机，如 3/17）
- **数据卡片**：当前 CH、角度、上/下限度、已用时间
- **触摸输入**：XPT2046 控制器（软件 SPI 位翻转 GPIO），压力判按下，接入 `lv_port_indev`

### 4. 上位机串口工具（[Tools/SerialTool/](Tools/SerialTool/)）

基于 PyQt5 的 Windows 串口接收与 YAML 导出工具：

- **串口连接**：支持常用波特率（9600 ~ 921600），自动枚举可用串口
- **数据解析**：自动识别 STM32 输出的 `servo_limits[]` 标定报告，解析出每路舵机的通道号、中心角、上下限度
- **YAML 模板合并**：加载参考 `servo_config.yaml`，将标定结果按 `channel_idx` 自动匹配写入对应舵机的 `start_deg` / `end_deg` / `temp_deg`，保持原始数据的大小方向关系
- **一键导出**：输出带时间戳的 YAML 配置文件，可直接用于后续舵机控制

### 5. 其他外设功能

- **数码管**：TM1637 4 位数码管（软件 I²C）
- **继电器控制**：KEY1 触发，控制外部电源等设备，上电默认断开（NC 闭合，负载带电）
- **按键输入**：KEY1 继电器、KEY2 启动标定、KEY3 重复上传标定结果
- **CAN**：CAN1 已初始化（500 kbit/s），为机械底盘通信预留接口
- **双模日志输出**：编译时可通过宏切换 UART1 或 SEGGER RTT

---

## 硬件平台

| 项目 | 型号 / 参数 |
|------|-------------|
| MCU | STM32F407VETx, 168 MHz |
| 舵机驱动 | PCA9685 (I²C, 27 路) |
| 电流检测 | INA229 (SPI, 20 位 Δ-Σ ADC) |
| LCD | 2.8" ILI9341 SPI 240×320（LVGL 8.3 GUI） |
| 触摸屏 | XPT2046（软件 SPI，压力判按下） |
| 数码管 | TM1637 4 位（软件 I²C） |
| CAN | CAN1 500 kbit/s（机械底盘，预留） |
| 调试输出 | UART1 (PA9, 115200 8N1) / SEGGER RTT 双模 |
| 按键 | KEY1 (PA0) 继电器, KEY2 (PA1) 启动标定, KEY3 (PA4) 重复上传结果 |
| LED | LED_BLUE (PE5) 标定状态指示 |

---

## 软件架构

```
STM32F407 (ARMCLANG V6.23)
├── FreeRTOS (CMSIS-RTOS v2)
├── HAL 驱动层
├── BSP 板级支持包
│   ├── servo / servo_actuator    PCA9685 舵机驱动
│   ├── servo_config              YAML 生成的舵机参数表 (start/end/temp/coef)
│   ├── ina229 / current          INA229 电流采集（Current_Task，~100 Hz）
│   ├── servo_calib               标定状态机 + 数据表
│   ├── sweep                     扫频扫描模块（独立备用）
│   ├── app_servo                 扫频/标定任务 (App_sweep_task)
│   ├── app_lcd + lvgl            LVGL 8.3 GUI 显示任务（仪表盘 + 卡片）
│   ├── bsp_LCD_ILI9341           ILI9341 驱动
│   ├── bsp_XPT2046               触摸驱动（软件 SPI）
│   ├── keys                      按键扫描（消抖，信号量触发）
│   ├── relay / app_relay         继电器控制
│   ├── tm1637 / myiic            数码管 + 软件 I²C
│   └── bsp_log                   日志输出（RTT / UART 双模）
├── SEGGER_RTT                    RTT 调试组件
├── sw/                           机械结构设计（SolidWorks: board.SLDPRT / board.STL）
└── Tools/
    └── SerialTool/               PyQt5 上位机串口工具
        ├── main.py               主程序
        └── requirements.txt      依赖清单
```

---

## FreeRTOS 任务一览

| 任务 | 功能 | 优先级 | 栈 |
|------|------|--------|-----|
| `LCD_TASK` | LVGL GUI 渲染 + 状态刷新（表盘/进度/CH/限位/耗时） | osPriorityNormal | 4 KB |
| `Current_Task` | INA229 电流采集（约 100 Hz） | osPriorityAboveNormal | 静态 512 B |
| `RELAY_TASK` | 继电器控制（KEY1 信号量触发） | osPriorityBelowNormal | 512 B |
| `KEY_SCAN` | 按键扫描消抖，释放 Key1/2/3 信号量 | osPriorityLow | 512 B |
| `SWEEP` | 舵机扫频标定状态机 | osPriorityHigh | 静态 1 KB |

---

## 标定流程

```
按 KEY2 启动
  │
  ├─ LED_BLUE 亮，TIM7 计时器启动 (SweepTimer)
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
      ├─ UART 输出总耗时 (秒)
      ├─ LED_BLUE 灭
      └─ 按 KEY3 可重复上传标定结果
```

---

## 舵机配置

27 路舵机参数由 `servo_config.h`（从 `servo_config.yaml` 生成）定义，含原始限位角 `start_deg` / `end_deg`（方向敏感）、默认角 `temp_deg` 与方向系数 `coefficient`（当前全为 1，正向）。标定时**上限度 = max(start, end)**，**下限度 = min(start, end)**。

| Idx | CH | Start | End | Temp | 名称 / 说明 |
|-----|----|-------|-----|------|-------------|
| 0 | 0 | 220 | 70 | 135 | L_eyebrow_head_ver 左眉头 上——下 |
| 1 | 1 | 50 | 200 | 135 | R_eyebrow_head_ver 右眉头 上——下 |
| 2 | 2 | 105 | 205 | 135 | L_eyebrow_head_hor 左眉头 外——内 |
| 3 | 3 | 165 | 65 | 135 | R_eyebrow_head_hor 右眉头 外——内 |
| 4 | 4 | 85 | 135 | 135 | L_eyebrow_tail 左眉尾 上——下 |
| 5 | 5 | 185 | 135 | 135 | R_eyebrow_tail 右眉尾 上——下 |
| 6 | 6 | 170 | 100 | 135 | L_eyebrow_hor 左眉 前——后 |
| 7 | 7 | 100 | 170 | 135 | R_eyebrow_hor 右眉 前——后 |
| 8 | 8 | 120 | 230 | 140 | L_upper_eyelid 左上眼皮 开——合 |
| 9 | 9 | 145 | 35 | 125 | R_upper_eyelid 右上眼皮 开——合 |
| 10 | 10 | 160 | 105 | 135 | L_vertical_eyeball 左眼球 上——下 |
| 11 | 11 | 110 | 165 | 135 | R_vertical_eyeball 右眼球 上——下 |
| 12 | 12 | 150 | 115 | 135 | L_horizontal_eyeball 左眼球 内——外 |
| 13 | 13 | 120 | 155 | 135 | R_horizontal_eyeball 右眼球 内——外 |
| 14 | 14 | 95 | 135 | 135 | R_cheek 右脸颊 下——上 |
| 15 | 15 | 175 | 135 | 135 | L_cheek 左脸颊 上——下 |
| 16 | 16 | 95 | 190 | 147 | mid_upper_lip_ver 中上嘴唇 上——下 |
| 17 | 17 | 160 | 200 | 190 | mid_upper_lip_hor 中上嘴唇 前——后 |
| 18 | 18 | 185 | 155 | 165 | mid_lower_lip_hor 中下嘴唇 前——后 |
| 19 | 19 | 200 | 100 | 155 | mid_lower_lip_ver 中下嘴唇 上——下 |
| 20 | 20 | 160 | 70 | 115 | upper_lip 上嘴唇 上——下 |
| 21 | 21 | 60 | 160 | 105 | lower_lip 下嘴唇 上——下 |
| 22 | 22 | 160 | 100 | 135 | R_upper_mouth 右嘴角 前——后 |
| 23 | 23 | 180 | 90 | 135 | L_lower_mouth 左嘴角 上——下 |
| 24 | 24 | 105 | 165 | 130 | L_upper_mouth 左嘴角 前——后 |
| 25 | 25 | 90 | 180 | 135 | R_lower_mouth 右嘴角 上——下 |
| 26 | 26 | 85 | 150 | 95 | chin_vertical 下巴 合——开 |

> **HW_CONFIG_LOW** 模式下，以下舵机被跳过（不参与标定，限位直接设为 0° ~ 270°）：
> 眉毛水平 (idx 2, 3, 6, 7)、眉尾 (idx 4, 5)、脸颊 (idx 14, 15)、嘴唇 (idx 20, 21)，共 10 路 → 实际标定 17 路。

---

## 关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| `CALIB_HARD_MIN` | 0° | 机械下限 |
| `CALIB_HARD_MAX` | 270° | 机械上限 |
| `CALIB_STEP_UP_DEG` | 2° | 上限搜索步长（上下限搜索共用） |
| `CALIB_STEP_DOWN_DEG` | 5° | 头文件定义，当前任务未使用 |
| 堵转电流阈值 | 0.6 A | `App_sweep_task` 判定值（`CALIB_STALL_CUR_A` 参考值 0.8 A） |
| 每步动作间隔 | 50 ms | 移动 + 等待（`osDelay(50)`） |
| 归中等待 | 500 ms | 每次归中后静置 |
| TIM7 计时 | 溢出 1 Hz | CNT_CLK 10 kHz，扫频总耗时计时器 |
| LVGL 内存池 | 48 KB | `LV_MEM_SIZE`，16 bit RGB565 双缓冲 |
| LCD 任务刷新 | 500 ms / 200 ms | 状态卡片 / 电流表盘定时刷新 |

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

## 机械结构

[sw/](sw/) 目录存放机械底盘结构设计文件（SolidWorks 格式）：

| 文件 | 说明 |
|------|------|
| `board.SLDPRT` | 底盘三维模型 |
| `board.STL` | 三维打印用网格模型 |

CAN1 接口已初始化（500 kbit/s），预留用于底盘通信。

---

## 目录结构

```
Servo_limit_detect/
├── BSP/                            # 板级支持包（源码）
│   ├── Inc/                        # 头文件
│   │   ├── servo.h                 # 舵机限位数据结构
│   │   ├── servo_config.h          # YAML 生成的舵机参数表
│   │   ├── servo_calib.h           # 标定状态机
│   │   ├── sweep.h                 # 扫频模块
│   │   ├── servo_actuator.h        # PCA9685 驱动
│   │   ├── current.h / ina229.h    # INA229 电流采集
│   │   ├── app_servo.h / app_lcd.h # 标定 / 显示任务接口
│   │   ├── bsp_log.h               # 日志输出接口
│   │   ├── bsp_LCD_ILI9341.h       # ILI9341 驱动
│   │   ├── bsp_XPT2046.h           # 触摸驱动
│   │   ├── myiic.h / tm1637.h      # 软件 I²C / 数码管
│   │   └── keys.h / relay.h / ...  # 其他外设
│   ├── Src/                        # 源文件
│   │   ├── servo_calib.c           # 标定实现 + 数据表
│   │   ├── sweep.c                 # 扫频实现（独立备用）
│   │   ├── app_servo.c             # 扫频 FreeRTOS 任务
│   │   └── ...
│   └── lvgl/                       # LVGL 8.3 图形库
│       ├── lv_conf.h               # LVGL 配置（内存池 / 字体 / 组件）
│       ├── porting/                # lv_port_disp / lv_port_indev 移植
│       ├── src/                    # LVGL 源码
│       └── demos/                  # 官方演示（可开启）
├── Core/                           # STM32CubeMX 生成
│   ├── Inc/                        # HAL 配置 / FreeRTOSConfig / 外设头文件
│   └── Src/                        # main, 中断, 外设初始化, freertos, tim(扫频计时)
├── Drivers/                        # HAL 库 + CMSIS
├── MDK-ARM/                        # Keil MDK 工程
├── Middlewares/                    # FreeRTOS
├── SEGGER_RTT/                     # RTT 源码
├── sw/                             # 机械结构设计（SolidWorks）
└── Tools/
    └── SerialTool/                 # 上位机串口工具
        ├── main.py                 # PyQt5 GUI 主程序
        └── requirements.txt        # Python 依赖
```

---

## 构建

Keil MDK-ARM 打开 `MDK-ARM/Servo_limit_detect.uvprojx`，ARMCLANG V6.23 编译。

> **注意**：
> - 需勾选 **Use MicroLIB**（Target → Code Generation）。
> - LVGL 参数（内存池大小、字体、组件使能）在 `BSP/lvgl/lv_conf.h` 中配置。
> - `HW_CONFIG_LOW` 宏在 `BSP/Inc/servo_calib.h` 中切换全 27 路 / 17 路标定。
