#include "bsp_XPT2046.h"
#include "string.h"

/**
 * @file    bsp_XPT2046.c
 * @brief   XPT2046 触摸控制器驱动（软件 SPI / 位翻转 GPIO）
 *
 * 移植自标准例程《29-1 显示屏_2.8寸_触摸测试_XPT2046》。
 * 与原例程的差异：去掉了 W25Q128 校准数据存储依赖，校准参数改为硬编码默认值。
 */

/******************************* 结构体 *******************************/
typedef struct
{
    uint8_t  EN;          /* 触摸检测开关, 0_关闭, 1_打开 */
    uint16_t lcdX;        /* 当前按下的 LCD 坐标 X */
    uint16_t lcdY;        /* 当前按下的 LCD 坐标 Y */
    uint16_t adcX;        /* 最后读取的 X 方向 ADC 值（已滤波） */
    uint16_t adcY;        /* 最后读取的 Y 方向 ADC 值（已滤波） */
    uint16_t lcdWidth;    /* LCD 宽度像素 */
    uint16_t lcdHeight;   /* LCD 高度像素 */
    float    xfac;        /* X 轴比例系数 */
    float    yfac;        /* Y 轴比例系数 */
    short    xoff;        /* X 轴像素偏移 */
    short    yoff;        /* Y 轴像素偏移 */
    uint8_t  dir;         /* 显示方向, 0_竖屏, 1_横屏 */
} xXPT2046_TypeDey;

static xXPT2046_TypeDey xXPT2046;

/******************************* 宏定义 *******************************/
#define XPT2046_CHANNEL_X   0x90   /* 控制字：检测通道 Y+ 电压值 */
#define XPT2046_CHANNEL_Y   0xD0   /* 控制字：检测通道 X+ 电压值 */
#define XPT2046_CHANNEL_Z1  0xB0   /* 控制字：压力检测 Z1 */
#define XPT2046_CHANNEL_Z2  0xC0   /* 控制字：压力检测 Z2 */

/* 无 IRQ 时的按下判据：压力 = 4095-(Z2-Z1)。未按时接近 0，按压时变大。
 * 低于此值视为未按下；可据屏上 Z= 读数微调（轻按漏识别则调小，误触则调大）。 */
#define XPT2046_Z_THRESHOLD 500

#define IRQ_READ  ( XPT2046_IRQ_GPIO -> IDR & XPT2046_IRQ_PIN )      /* 闲时高电平，按下时低电平 */
#define CS_HIGH   ( XPT2046_CS_GPIO  -> BSRR = XPT2046_CS_PIN )
#define CS_LOW    ( XPT2046_CS_GPIO  -> BSRR = XPT2046_CS_PIN << 16 )
#define CLK_HIGH  ( XPT2046_CLK_GPIO -> BSRR = XPT2046_CLK_PIN )
#define CLK_LOW   ( XPT2046_CLK_GPIO -> BSRR = XPT2046_CLK_PIN << 16 )
#define MOSI_1    ( XPT2046_MOSI_GPIO-> BSRR = XPT2046_MOSI_PIN )
#define MOSI_0    ( XPT2046_MOSI_GPIO-> BSRR = XPT2046_MOSI_PIN << 16 )
#define MISO      ( ((XPT2046_MISO_GPIO -> IDR) & XPT2046_MISO_PIN) ? 1 : 0 )

/******************************* 本地函数声明 *******************************/
static void      delayUS(uint32_t us);
static void      sendCMD(uint8_t cmd);
static uint16_t  receiveData(void);
static int16_t   readADC_X(void);
static int16_t   readADC_Y(void);
static uint8_t   readAdcXY(void);
static void      adcXYToLcdXY(void);

/******************************* 本地函数实现 *******************************/
/* 粗略 US 延时，减少移植时对外部文件的依赖
 * 注意：必须用 volatile + __NOP，否则在 -O3 下空循环会被优化掉，导致软件 SPI 时序失效 */
static void delayUS(uint32_t us)
{
    volatile uint32_t n = us * 20;   /* 168MHz 下粗略 ~1us，与 bsp_LCD_ILI9341.c 的 delay_us 同法 */
    while (n--)
        __NOP();
}

/* 写入命令字
 * Cmd：0x90_通道Y+选择控制字, 0xd0_通道X+选择控制字
 * XPT2046 时序：CLK 闲时低电平，上升沿采样数据，下降沿改变数据 */
static void sendCMD(uint8_t cmd)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        ((cmd >> (7 - i)) & 0x01) ? MOSI_1 : MOSI_0;   /* 高位先行 */
        delayUS(1);
        CLK_HIGH;
        delayUS(1);
        CLK_LOW;
    }
}

/* 读取返回数据（紧跟 sendCMD 之后），返回 12 位 ADC 结果 */
static uint16_t receiveData(void)
{
    uint16_t usBuf = 0;

    CLK_HIGH;              /* 给一个时钟，清除 BUSY（AD 转换约需 6us） */
    delayUS(5);
    CLK_LOW;
    delayUS(5);

    for (uint8_t i = 0; i < 12; i++)
    {
        usBuf = usBuf << 1;
        CLK_HIGH;
        delayUS(1);
        usBuf |= MISO;     /* 高位先行 */
        CLK_LOW;
        delayUS(1);
    }

    return usBuf;
}

static int16_t readADC_X(void)
{
    if (xXPT2046.dir == 0)    sendCMD(XPT2046_CHANNEL_Y);
    if (xXPT2046.dir == 1)    sendCMD(XPT2046_CHANNEL_X);
    return receiveData();
}

static int16_t readADC_Y(void)
{
    if (xXPT2046.dir == 0)    sendCMD(XPT2046_CHANNEL_X);
    if (xXPT2046.dir == 1)    sendCMD(XPT2046_CHANNEL_Y);
    return receiveData();
}

/* 获取触摸屏按下时 X、Y 的 ADC 值并滤波（去最小/最大后求均值），存到全局结构体 */
static uint8_t readAdcXY(void)
{
    static uint8_t  cnt = 0;
    static uint16_t xSum = 0, ySum = 0;
    static int16_t  xyArray[2][10] = {{0}, {0}};
    int32_t xMin, xMax, yMin, yMax;

    cnt = 0;
    xSum = 0;
    ySum = 0;
    memset(xyArray, 0, sizeof(xyArray));

    while (cnt < 4)                        /* 无条件采集 4 次（不再依赖 IRQ 门） */
    {
        xyArray[0][cnt] = readADC_X();
        xyArray[1][cnt] = readADC_Y();
        cnt++;
    }

    if (cnt == 0)
        return 0;

    xMax = xMin = xyArray[0][0];
    yMax = yMin = xyArray[1][0];
    for (uint8_t i = 1; i < cnt; i++)
    {
        if (xyArray[0][i] < xMin)    xMin = xyArray[0][i];
        if (xyArray[0][i] > xMax)    xMax = xyArray[0][i];
        if (xyArray[1][i] < yMin)    yMin = xyArray[1][i];
        if (xyArray[1][i] > yMax)    yMax = xyArray[1][i];
    }

    for (uint8_t i = 0; i < cnt; i++)
    {
        xSum += xyArray[0][i];
        ySum += xyArray[1][i];
    }

    xXPT2046.adcX = (xSum - xMin - xMax) >> 1;   /* 去最小/最大后求均值 */
    xXPT2046.adcY = (ySum - yMin - yMax) >> 1;

    return 1;
}

/* 把 ADC 电压值换算成 LCD 坐标，并限制范围 */
static void adcXYToLcdXY(void)
{
    int16_t lcdX = xXPT2046.adcX * xXPT2046.xfac + xXPT2046.xoff;
    int16_t lcdY = xXPT2046.adcY * xXPT2046.yfac + xXPT2046.yoff;

    if (lcdX < 0)                     lcdX = 0;
    if (lcdX > xXPT2046.lcdWidth)     lcdX = xXPT2046.lcdWidth;
    if (lcdY < 0)                     lcdY = 0;
    if (lcdY > xXPT2046.lcdHeight)    lcdY = xXPT2046.lcdHeight;

    xXPT2046.lcdX = lcdX;
    xXPT2046.lcdY = lcdY;
}

/******************************* 对外函数实现 *******************************/
void XPT2046_Init(uint16_t lcdWidth, uint16_t lcdHeight, uint8_t dir)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    xXPT2046.lcdWidth  = lcdWidth;
    xXPT2046.lcdHeight = lcdHeight;
    xXPT2046.dir       = dir;

    /* 使能 GPIO 时钟（LCD_Init 已使能，重复置位无副作用） */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* CS 引脚 */
    GPIO_InitStruct.Pin   = XPT2046_CS_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(XPT2046_CS_GPIO, &GPIO_InitStruct);
    CS_HIGH;                                        /* 拉高片选，防止误操作 */

    /* CLK */
    GPIO_InitStruct.Pin = XPT2046_CLK_PIN;
    HAL_GPIO_Init(XPT2046_CLK_GPIO, &GPIO_InitStruct);

    /* MOSI */
    GPIO_InitStruct.Pin = XPT2046_MOSI_PIN;
    HAL_GPIO_Init(XPT2046_MOSI_GPIO, &GPIO_InitStruct);

    /* MISO */
    GPIO_InitStruct.Pin  = XPT2046_MISO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(XPT2046_MISO_GPIO, &GPIO_InitStruct);

    /* IRQ（轮询，不使用中断） */
    GPIO_InitStruct.Pin  = XPT2046_IRQ_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(XPT2046_IRQ_GPIO, &GPIO_InitStruct);

    CLK_LOW;                                        /* CLK 闲时低电平 */
    MOSI_0;                                         /* MOSI 闲时低电平 */
    CS_LOW;                                         /* 拉低片选，使 XPT2046 开始通信 */

    /* 硬编码默认校准参数（替代原例程的 W25Q128 存储） */
    if (xXPT2046.dir == 0)                          /* 竖屏 */
    {
        xXPT2046.xfac = 0.0675f;
        xXPT2046.yfac = 0.091f;
        xXPT2046.xoff = -18;
        xXPT2046.yoff = -19;
    }
    else                                            /* 横屏（备用） */
    {
        xXPT2046.xfac = 0.09184f;
        xXPT2046.yfac = 0.077956f;
        xXPT2046.xoff = 338;
        xXPT2046.yoff = -45;
    }

    XPT2046_Cmd(1);                                 /* 打开触摸检测 */
}

uint8_t XPT2046_IsPressed(void)
{
    if (xXPT2046.EN == 0)             /* 触摸检测开关关闭 */
        return 0;

    /* 按下判据：本板 PENIRQ(PE4) 未接出/不可用，改用 Z 压力判断。
     * 未按时压力接近 0，按压时压力明显变大。 */
    if (XPT2046_GetPressure() < XPT2046_Z_THRESHOLD)
        return 0;

    /* 已按下：采集坐标 + 去抖 */
    readAdcXY();
    adcXYToLcdXY();
    uint16_t x1 = xXPT2046.lcdX;
    uint16_t y1 = xXPT2046.lcdY;

    readAdcXY();
    adcXYToLcdXY();
    uint16_t x2 = xXPT2046.lcdX;
    uint16_t y2 = xXPT2046.lcdY;

    /* 计算两次采值差，超过 3 像素判为抖动，返回未按下 */
    uint8_t x = (x1 > x2) ? (x1 - x2) : (x2 - x1);
    uint8_t y = (y1 > y2) ? (y1 - y2) : (y2 - y1);

    if (x > 3 || y > 3)
        return 0;

    xXPT2046.lcdX = (x1 + x2) >> 1;
    xXPT2046.lcdY = (y1 + y2) >> 1;

    return 1;                         /* 按下中 */
}

uint16_t XPT2046_GetX(void)
{
    return xXPT2046.lcdX;
}

uint16_t XPT2046_GetY(void)
{
    return xXPT2046.lcdY;
}

/* 读触摸压力：压力 = 4095 - (Z2-Z1)。按压越强值越大，未按时接近 0。
 * 用于在 PENIRQ 未接出时替代 IRQ 做按下判断。 */
uint16_t XPT2046_GetPressure(void)
{
    uint16_t z1, z2, diff;

    sendCMD(XPT2046_CHANNEL_Z1);
    z1 = receiveData();
    sendCMD(XPT2046_CHANNEL_Z2);
    z2 = receiveData();

    diff = (z2 > z1) ? (z2 - z1) : 0;
    return 4095 - diff;
}

void XPT2046_Cmd(uint8_t status)
{
    xXPT2046.EN = (status != 0) ? 1 : 0;
}

void XPT2046_TouchDown(void)
{
    /* 空钩子，用户自行扩展 */
}

void XPT2046_TouchUp(void)
{
    /* 空钩子，用户自行扩展 */
}
