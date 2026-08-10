#include "bsp_LCD_ILI9341.h"
#include "font.h"
#include "string.h"
#include "stdio.h"
#include "stm32f407xx.h"







/*****************************************************************************
 ** 参数定义
****************************************************************************/
#define LCD_BL_ON    LCD_BL_GPIO-> BSRR = LCD_BL_PIN;       // 打开背光，拉高电平
#define LCD_BL_OFF   LCD_BL_GPIO-> BSRR = LCD_BL_PIN << 16; // 关闭背光，拉低电平


static void setCursor(uint16_t Xpos, uint16_t Ypos);        // 设置光标


typedef struct                                              // LCD重要参数结构体
{
    uint16_t width;                                         // LCD 宽度
    uint16_t height;                                        // LCD 高度
    uint16_t id;                                            // LCD ID
    uint8_t  dir;                                           // 横竖屏方向控制，0-竖屏，1-横屏
    uint8_t  FlagInit;                                      // 初始化完成标志
} xLCD_TypeDef;
static xLCD_TypeDef xLCD = {0};                             // 存储LCD重要参数

// 使用NOR/SRAM的 Bank1.sector1,地址位HADDR[27,26]=11 A6作为数据命令选择线
// 注意读写时STM32内部会右移一位地址! 111 1110=0X7E
volatile typedef struct                                     // LCD地址结构体
{
    uint16_t LCD_REG;
    uint16_t LCD_RAM;
} LCD_TypeDef;
#define LCD       ((LCD_TypeDef *) 0x6001FFFE)              // (0x60000000 | 0x0001FFFE)



// us延时
static void delay_us(volatile uint32_t times)   // 定义一个us延时函数，供本文件内部调用; 非精确延时，仅供粗略时序使用;
{
    times = times * 20;
    while (--times)
        __NOP();
}



/******************************************************************************
 * 函数名: delay_ms
 * 功  能: ms 延时函数
 * 注  意: 1-系统时钟168MHz
 *          2-勾选: Options/ c++ / One ELF Section per Function
 *          3-优化选项: Level 3(-O3)
 * 参  数: uint32_t  ms  延时值
 * 返回值: 无
 ******************************************************************************/
static volatile uint32_t ulTimesMS;         // 使用volatile关键字，防止编译器过度优化
static void delay_ms(uint16_t ms)
{
    ulTimesMS = ms * 16500;
    while (ulTimesMS)
        ulTimesMS--;                        // 可被外部中断打破，防止死循环被过度优化
}



// 读寄存器;
uint16_t  readReg(uint16_t  LCD_Reg)
{
    LCD->LCD_REG = LCD_Reg;                 // 写入要读的寄存器地址
    delay_us(5);
    return LCD->LCD_RAM;                    // 返回读取值
}



// BGR转RGB值; 返回值: RGB格式的颜色值
uint16_t  LCD_BGR2RGB(uint16_t  c)
{
    uint16_t   r, g, b, rgb;
    b = (c >> 0) & 0x1f;
    g = (c >> 5) & 0x3f;
    r = (c >> 11) & 0x1f;
    rgb = (b << 11) + (g << 5) + (r << 0);
    return (rgb);
}



/******************************************************************
 * 函数名: LCD_Init
 * 功  能: 初始化LCD，驱动芯片ILI9341
 * 参  数: 无
 * 注  意: 无
 *****************************************************************/
void LCD_Init(void)
{
    xLCD.FlagInit = 0;                                       // LCD初始化成功标志; 0-未初始化，1-初始化成功

    /** 1-初始化GPIO **/
    GPIO_InitTypeDef  GPIO_InitStruct = {0};                 // GPIO引脚功能配置结构体

                                      // HAL库 配置
    // 使能GPIO端口时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();                            // 使能GPIOA端口时钟
    __HAL_RCC_GPIOB_CLK_ENABLE();                            // 使能GPIOB端口时钟
    __HAL_RCC_GPIOC_CLK_ENABLE();                            // 使能GPIOC端口时钟
    __HAL_RCC_GPIOD_CLK_ENABLE();                            // 使能GPIOD端口时钟
    __HAL_RCC_GPIOE_CLK_ENABLE();                            // 使能GPIOE端口时钟
    /// 初始化背光-控制引脚
    GPIO_InitStruct.Pin   = LCD_BL_PIN;                      // 引脚编号
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;             // 引脚工作模式: 推挽输出
    GPIO_InitStruct.Pull  = GPIO_PULLUP;                     // 内部上拉电阻
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;       // 引脚翻转速率: 最高
    HAL_GPIO_Init(LCD_BL_GPIO, &GPIO_InitStruct);            // 初始化
    // 通道数据 GPIOD配置
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;                  // 引脚工作模式: 复用推挽输出
    GPIO_InitStruct.Pull = GPIO_PULLUP;                      // 内部上拉电阻
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;       // 引脚翻转速率: 最高
    GPIO_InitStruct.Alternate = GPIO_AF12_FSMC;              // 引脚功能: FSMC
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);                  // 初始化
    // 通道数据 GPIOE配置
    GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15 ;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;                  // 引脚工作模式: 复用推挽输出
    GPIO_InitStruct.Pull = GPIO_PULLUP;                      // 内部上拉电阻
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;       // 引脚翻转速率: 最高
    GPIO_InitStruct.Alternate = GPIO_AF12_FSMC;              // 引脚功能: FSMC
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);                  // 初始化


    /** 2-FMSC 初始化,使用FSMC模拟8080接口 **/

    // 使能FSMC时钟
    RCC->AHB3ENR |= RCC_AHB3ENR_FSMCEN;                      // 使能复用FSMC的时钟

    // 每个BANK有寄存器1~4, 要操作3个寄存器
    // 区域1:BTCR0和1、BWTR0;
    // 区域2:BTCR2和3、BWTR1;
    // 区域3:BTCR4和5、BWTR2;
    // 区域4:BTCR6和7、BWTR3;
    FSMC_Bank1->BTCR[0]   = 0X00000000;
    FSMC_Bank1->BTCR[0 + 1] = 0X00000000;
    FSMC_Bank1E->BWTR[0]  = 0X00000000;

    //配置BCR寄存器    使用异步模式
    FSMC_Bank1->BTCR[0] |= 0x01 << 12;        // 存储器写使能
    FSMC_Bank1->BTCR[0] |= 0x01 << 14;        // 读写使用不同的时序
    FSMC_Bank1->BTCR[0] |= 0x01 << 4;         // 存储器数据宽度为16bit

    //读时序控制寄存器
    FSMC_Bank1->BTCR[0 + 1] |= 0x00 << 28;    // 模式A
    FSMC_Bank1->BTCR[0 + 1] |= 0X0F << 0;     // 地址建立时间(ADDSET)为15个HCLK 1/168M=6ns*15=90ns
    FSMC_Bank1->BTCR[0 + 1] |= 0x3C << 8;     // 数据保持时间(DATAST)为60个HCLK    =6*60=360ns
    //写时序控制寄存器
    FSMC_Bank1E->BWTR[0] |= 0x00 << 28;       // 模式A
    FSMC_Bank1E->BWTR[0] |= 0x09 << 0;        // 地址建立时间(ADDSET)为9个HCLK=54ns
    FSMC_Bank1E->BWTR[0] |= 0x08 << 8;        // 数据保持时间(DATAST)为6ns*9个HCLK=54ns

    //使能BANK1区域1
    FSMC_Bank1->BTCR[0] |= 0x01;              // 使能BANK1区域1

    delay_ms(50);                             // delay 50 ms
    LCD->LCD_REG = 0x0000;                    // 写入要写的寄存器地址
    LCD->LCD_RAM = 0x0000;                    // 写入数据
    delay_ms(50);                             // delay 50 ms
    xLCD.id = readReg(0x0000);

    LCD->LCD_REG = 0XD3;                      // 读取9341 ID的指令
    xLCD.id = LCD->LCD_RAM;                   // dummy read
    xLCD.id = LCD->LCD_RAM;                   // 读到0X00
    xLCD.id = LCD->LCD_RAM;                   // 读取93
    xLCD.id <<= 8;
    xLCD.id |= LCD->LCD_RAM;                  // 读取41
//    printf("显示屏:                  芯片ID: %x\r\n", xLCD.id);  // 打印显示屏的芯片ID

    //根据ID调整写时序控制寄存器参数
    FSMC_Bank1E->BWTR[0] &= ~(0XF << 0);      // 地址建立时间(ADDSET)清零
    FSMC_Bank1E->BWTR[0] |= 2 << 8;           // 数据保持时间(DATAST)为6ns*3个HCLK=18ns
    FSMC_Bank1E->BWTR[0] &= ~(0XF << 8);      // 数据保持时间清零
    FSMC_Bank1E->BWTR[0] |= 3 << 0;           // 地址建立时间(ADDSET)为3个HCLK =18ns

    /** 3-ILI9341 寄存器配置 **/

    // 核心参数配置，不要修改，否则可能不工作
    LCD->LCD_REG = 0xCF;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0xC1;
    LCD->LCD_RAM = 0X30;
    LCD->LCD_REG = 0xED;
    LCD->LCD_RAM = 0x64;
    LCD->LCD_RAM = 0x03;
    LCD->LCD_RAM = 0X12;
    LCD->LCD_RAM = 0X81;
    LCD->LCD_REG = 0xE8;
    LCD->LCD_RAM = 0x85;
    LCD->LCD_RAM = 0x10;
    LCD->LCD_RAM = 0x7A;
    LCD->LCD_REG = 0xCB;
    LCD->LCD_RAM = 0x39;
    LCD->LCD_RAM = 0x2C;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x34;
    LCD->LCD_RAM = 0x02;
    LCD->LCD_REG = 0xF7;
    LCD->LCD_RAM = 0x20;
    LCD->LCD_REG = 0xEA;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_REG = 0xC0;  // Power control
    LCD->LCD_RAM = 0x1B;  // VRH[5:0]
    LCD->LCD_REG = 0xC1;  // Power control
    LCD->LCD_RAM = 0x01;  // SAP[2:0];BT[3:0]
    LCD->LCD_REG = 0xC5;  // VCM control
    LCD->LCD_RAM = 0x30;  // 3F
    LCD->LCD_RAM = 0x30;  // 3C
    LCD->LCD_REG = 0xC7;  // VCM control2
    LCD->LCD_RAM = 0XB7;
    LCD->LCD_REG = 0x36;  // Memory Access Control
    LCD->LCD_RAM = 0x48;
    LCD->LCD_REG = 0x3A;
    LCD->LCD_RAM = 0x55;
    LCD->LCD_REG = 0xB1;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x1A;
    LCD->LCD_REG = 0xB6;  // Display Function Control
    LCD->LCD_RAM = 0x0A;
    LCD->LCD_RAM = 0xA2;
    LCD->LCD_REG = 0xF2;  // 3Gamma Function Disable
    LCD->LCD_RAM = 0x00;
    LCD->LCD_REG = 0x26;  // Gamma curve selected
    LCD->LCD_RAM = 0x01;
    LCD->LCD_REG = 0xE0;  // Set Gamma
    LCD->LCD_RAM = 0x0F;
    LCD->LCD_RAM = 0x2A;
    LCD->LCD_RAM = 0x28;
    LCD->LCD_RAM = 0x08;
    LCD->LCD_RAM = 0x0E;
    LCD->LCD_RAM = 0x08;
    LCD->LCD_RAM = 0x54;
    LCD->LCD_RAM = 0XA9;
    LCD->LCD_RAM = 0x43;
    LCD->LCD_RAM = 0x0A;
    LCD->LCD_RAM = 0x0F;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_REG = 0XE1;   // Set Gamma
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x15;
    LCD->LCD_RAM = 0x17;
    LCD->LCD_RAM = 0x07;
    LCD->LCD_RAM = 0x11;
    LCD->LCD_RAM = 0x06;
    LCD->LCD_RAM = 0x2B;
    LCD->LCD_RAM = 0x56;
    LCD->LCD_RAM = 0x3C;
    LCD->LCD_RAM = 0x05;
    LCD->LCD_RAM = 0x10;
    LCD->LCD_RAM = 0x0F;
    LCD->LCD_RAM = 0x3F;
    LCD->LCD_RAM = 0x3F;
    LCD->LCD_RAM = 0x0F;
    LCD->LCD_REG = 0x2B;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x01;
    LCD->LCD_RAM = 0x3f;
    LCD->LCD_REG = 0x2A;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0x00;
    LCD->LCD_RAM = 0xef;
    LCD->LCD_REG = 0x11;  // 退出睡眠模式

    delay_ms(120);
    LCD->LCD_REG = 0x29;  // 开显示

    /** 4-应用设置 **/

    LCD_SetDir(0);        // 设置显示的方向
    LCD_Fill(0, 0, xLCD.width, xLCD.height, BLACK);
    LCD_BL_ON;            // 开LCD背光
    xLCD.FlagInit = 1;
}



/******************************************************************
 * 函数名: LCD_SetDir
 * 功  能: 设置显示方向
 * 参  数: uint8_t dir     0-竖屏，1-横屏
 * 注  意: 若使用触摸，每次更改方向后，都需要重新校准
 *          各寄存器参考值: 0-竖屏, 3-横屏1, 5-竖屏2, 6-横屏2; 注意：若使用触摸，每次更改方向后，都需要重新校准
 * 返回值: 无
 *****************************************************************/
void LCD_SetDir(uint8_t dir)
{
    uint16_t  regval = 0;
    uint16_t  temp = 0;

    if (dir == 1)
        dir = 6;

    if (dir == 0 || dir == 3)         // 竖屏
    {
        xLCD.dir = 0;
        xLCD.width = LCD_WIDTH;
        xLCD.height = LCD_HEIGHT;
    }
    else                              // 横屏
    {
        xLCD.dir = 1;
        xLCD.width = LCD_HEIGHT;
        xLCD.height = LCD_WIDTH;
    }

    if (dir == 0) regval |= (0 << 7) | (0 << 6) | (0 << 5); // 竖屏,从上到下
    if (dir == 3) regval |= (1 << 7) | (1 << 6) | (0 << 5); // 从左到右,从下到上
    if (dir == 5) regval |= (0 << 7) | (1 << 6) | (1 << 5); // 从上到下,从右到左
    if (dir == 6) regval |= (1 << 7) | (0 << 6) | (1 << 5); // 从下到上,从左到右

    regval |= 0X08;
    LCD->LCD_REG = 0X36;               // 写入要写的寄存器地址
    LCD->LCD_RAM = regval;             // 写入数据

    // 确保xLCD.width对应X轴像素量，xLCD.height对应Y轴像素量，与真实硬件扫描方向一致。
    if (regval & 0X20)                 // 判断bit5是否为1
    {
        if (xLCD.width < xLCD.height)  // 交换X,Y
        {
            temp = xLCD.width;
            xLCD.width = xLCD.height;
            xLCD.height = temp;
        }
    }
    else
    {
        if (xLCD.width > xLCD.height)  // 交换X,Y
        {
            temp = xLCD.width;
            xLCD.width = xLCD.height;
            xLCD.height = temp;
        }
    }

    //设置X轴（水平方向）显示范围
    LCD->LCD_REG = 0X2A;                      // 列地址设置命令
    LCD->LCD_RAM = 0;                         // 起始列地址高8位
    LCD->LCD_RAM = 0;                         // 起始列地址低8位，从0开始
    LCD->LCD_RAM = (xLCD.width - 1) >> 8;     // 结束列地址高8位
    LCD->LCD_RAM = (xLCD.width - 1) & 0XFF;   // 结束列地址低8位
    // 设置Y轴（垂直方向）显示范围
    LCD->LCD_REG = 0X2B;                      // 页地址设置命令
    LCD->LCD_RAM = 0;                         // 起始页地址高8位
    LCD->LCD_RAM = 0;                         // 起始页地址低8位，从0开始
    LCD->LCD_RAM = (xLCD.height - 1) >> 8;    // 结束页地址高8位
    LCD->LCD_RAM = (xLCD.height - 1) & 0XFF;  // 结束页地址低8位
}



//设置光标位置; Xpos:横坐标、Ypos:纵坐标
static void setCursor(uint16_t  Xpos, uint16_t  Ypos)
{
    LCD->LCD_REG = 0X2A;           // 设置指令：设置x坐标
    LCD->LCD_RAM = Xpos >> 8;
    LCD->LCD_RAM = Xpos & 0XFF;
    LCD->LCD_REG = 0X2B;           // 设置指令：设置y坐标
    LCD->LCD_RAM = Ypos >> 8;
    LCD->LCD_RAM = Ypos & 0XFF;
}



/******************************************************************
 * 函数名: LCD_DrawPoint
 * 功  能: 画点函数
 * 参  数: x,y:    坐标
 *          _color: 此点的颜色
 * 注  意: 无
 *****************************************************************/
void LCD_DrawPoint(uint16_t  x, uint16_t  y, uint16_t _color)
{
    LCD->LCD_REG = 0X2A;        // 指令 设置x坐标
    LCD->LCD_RAM = x >> 8;      // x坐标 高8位
    LCD->LCD_RAM = x & 0XFF;    // x坐标 低8位

    LCD->LCD_REG = 0X2B;        // 指令 设置y坐标
    LCD->LCD_RAM = y >> 8;      // y坐标 高8位
    LCD->LCD_RAM = y & 0XFF;    // y坐标 低8位

    LCD->LCD_REG = 0X2C;        // 指令 开始写GRAM
    LCD->LCD_RAM = _color;      // 颜色值
}



/******************************************************************
 * 函数名: LCD_Fill
 * 功  能: 在指定区域内填充单一颜色
 * 参  数: uint16_t startX     左上角X坐标
 *          uint16_t startY     左上角Y坐标
 *          uint16_t endX       右下角X坐标
 *          uint16_t endY       右下角Y坐标
 *          uint16_t color      颜色值
 * 返回值: 无
 *****************************************************************/
void LCD_Fill(uint16_t startX, uint16_t startY, uint16_t endX, uint16_t endY, uint16_t color)
{
    uint16_t  xlen = 0;
    xlen = endX - startX + 1;
    for (uint16_t i = startY; i <= endY; i++)
    {
        setCursor(startX, i);                  // 设置光标位置
        LCD->LCD_REG = 0X2C;                   // 开始写GRAM
        for (uint16_t j = 0; j < xlen; j++)
            LCD->LCD_RAM = color;              // 颜色值
    }
}



/******************************************************************
 * 函数名: drawAscii
 * 功  能: 在指定位置显示一个字符
 * 参  数: uint16_t x,y     起始坐标
 *          uint8_t  num     要显示的字符:" "--->"~"
 *          uint8_t  size    字体大小 12/16/24/32
 *          uint32_t bColor  背景颜色
 *          uint32_t fColor  字体颜色
 * 注  意: 参考正点原子野火的代码修改
 *****************************************************************/
static void drawAscii(uint16_t x, uint16_t y, uint8_t num, uint8_t size, uint32_t bColor, uint32_t fColor)
{
    static uint8_t temp = 0;
    static uint8_t csize = 0;
    static uint16_t y0 = 0;

    y0 = y;

    csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);   // 得到字体一个字符对应点阵集所占的字节数
    num = num - ' ';                                          // 得到偏移后的值（ASCII字库是从空格开始取模，所以-' '就是对应字符的字库）
    for (uint8_t t = 0; t < csize; t++)
    {
        if (size == 12)         temp = aFontASCII12[num][t];  // 调用1206字体
        else if (size == 16)    temp = aFontASCII16[num][t];  // 调用1608字体
        else if (size == 24)    temp = aFontASCII24[num][t];  // 调用2412字体
        else if (size == 32)    temp = aFontASCII32[num][t];  // 调用3216字体
        else                    return;                       // 没有的字库

        for (uint8_t t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80) LCD_DrawPoint(x, y, fColor);     // 前景 画点
            else             LCD_DrawPoint(x, y, bColor);     // 背景 画点

            temp <<= 1;
            y++;
            if (y >= xLCD.height)    return;                  // 超出屏幕高度(纵)
            if ((y - y0) == size)
            {
                y = y0;
                x++;
                if (x >= xLCD.width) return;                  // 超出屏幕宽度(横)
                break;
            }
        }
    }
}



/******************************************************************
 * 函数名: drawGBK
 * 功  能: 在指定位置显示一个汉字
 * 参  数: uint16_t  x,y     起始坐标
 *          uint8_t * font    字库
 *          uint8_t   size    字体大小 12/16/24/32
 *          uint32_t  bColor  背景颜色
 *          uint32_t  fColor  字体颜色
 * 注  意: 无
 *****************************************************************/
//static void drawGBK(uint16_t x, uint16_t y, uint8_t *font, uint8_t size, uint32_t bColor, uint32_t fColor)
//{
//    static uint8_t temp = 0;
//    static uint16_t y0 = 0;
//    static uint8_t GBKData[128] = {0};
//    static uint8_t csize = 0;

//    csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size);         // 计算一个汉字字库数据所占的字节数
//    W25Q128_ReadFontData(font, size, GBKData);                  // 读取汉字点阵的数据内容

//    y0 = y;
//    for (uint8_t t = 0; t < csize; t++)
//    {
//        temp = GBKData[t];                                      // 得到GBK点阵数据
//        for (uint8_t t1 = 0; t1 < 8; t1++)
//        {
//            if (temp & 0x80)   LCD_DrawPoint(x, y, fColor);     // 若数据中的点，就画颜色
//            else               LCD_DrawPoint(x, y, bColor);     // 若数据中的无点，就画底色
//            temp <<= 1;
//            y++;
//            if ((y - y0) == size)
//            {
//                y = y0;
//                x++;
//                break;
//            }
//        }
//    }
//}



/******************************************************************************
 * 函数名: LCD_String
 * 功  能: 在LCD上显示字符串(支持英文、汉字)
 * 说  明: 英文：字模数据保存在font.h中，和程序一起保存在芯片内部Flash
 *          汉字：字模数据存在外部Flash中，即汉字字库在W25Q128中；
 *                魔女开发板的W25Q128中已烧录了4种字号大小的字模数据。
 * 参  数: uint16_t   x       字符区左上角X坐标
 *          uint16_t   y       字符区左上角y坐标
 *          char*      pFont   要显示的字符串指针
 *          uint8_t    size    字号大小：12 16 24 32
 *          uint32_t   fColor  字体颜色
 *          uint32_t   bColor  背景颜色
 * 返回值:  无
 * 注  意: 最后修改_2020年05月1日
 ******************************************************************************/
void LCD_String(uint16_t x, uint16_t y, char *pFont, uint8_t size, uint32_t fColor, uint32_t bColor)
{
    if (xLCD .FlagInit == 0) return;

    uint16_t xStart = x;

    if (size != 12 && size != 16 && size != 24 && size != 32)      // 检测字体大小
        size = 24;

    while (*pFont != 0)                                            // 循环读取字符串数据，直到'\0'时停止
    {
        if (x > (xLCD.width - size))                               // 换行判断，如果到达行末尾，就把光标换行
        {
            x = xStart;
            y = y + size;
        }
        if (y > (xLCD.height - size))                              // 换页判断，如果到达列末尾，就返回，不显示
            return;

        if (*pFont < 127)                                          // ASCII字符
        {
            drawAscii(x, y, *pFont, size, bColor, fColor);
            pFont++;
            x += size / 2;
        }
        else                                                       // 汉字显示
        {
//            drawGBK(x, y, (uint8_t *)pFont, size, bColor, fColor); // 重要: 如果用的不是魔女开发板的汉字字库, 需要修改或注释掉这一行, 这样就不影响ASCII英文字符的显示
//            pFont = pFont + 2;                                     // 下一个要显示的汉字在内存中的位置
//            x = x + size;                                          // 下一个要显示的汉字在屏幕上的X位置
        }
    }
}



/*****************************************************************
 * 函数名: LCD_DisplayOn
 * 功  能: 打开显示背光
 * 参  数: 无
 * 返回值: 无
*****************************************************************/
void LCD_DisplayOn(void)
{
    LCD->LCD_REG = 0X29;                // 开启显示
    LCD_BL_GPIO->BSRR |= LCD_BL_PIN;    // 打开背光
}



/*****************************************************************
 * 函数名: LCD_DisplayOff
 * 功  能: 关闭显示, 熄屏
 * 参  数: 无
 *
 * 返回值: 无
*****************************************************************/
void LCD_DisplayOff(void)
{
    LCD->LCD_REG = 0X28;                                // 关闭显示
    LCD_BL_GPIO->BSRR |= (uint32_t)LCD_BL_PIN << 16;    // 关闭背光
}



/******************************************************************
 * 函数名: LCD_GetWidth
 * 功  能: 获取屏幕的宽度。单位:像素
 * 注  意: 无
 * 参  数: 无
 * 返回值: uint16_t
 *****************************************************************/
uint16_t LCD_GetWidth(void)
{
    return xLCD.width;

}



/******************************************************************
 * 函数名: LCD_GetHeight
 * 功  能: 获取屏幕的高度。单位:像素
 * 注  意: 无
 * 参  数: 无
 * 返回值: uint16_t
 *****************************************************************/
uint16_t LCD_GetHeight(void)
{
    return xLCD.height;
}



/******************************************************************
 * 函数名: LCD_GetDir
 * 功  能: 获取屏幕的显示方向
 * 注  意: 无
 * 参  数: 无
 * 返回值: 0-竖屏，1-横屏
 *****************************************************************/
uint8_t  LCD_GetDir(void)
{
    return xLCD.dir;
}



/*****************************************************************
 * 函数名: LCD_Line
 * 功  能: 画线段
 * 参  数: xStart      起始x坐标
 *         yStart      起始y坐标
 *         xEnd        终点x坐标
 *         yEnd        终点y坐标
 *         color       颜色
 * 返回值: 无
******************************************************************/
void LCD_Line(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t _color)
{
    uint16_t  t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, uRow, uCol;
    delta_x = xEnd - xStart;                             // 计算坐标增量
    delta_y = yEnd - yStart;
    uRow = xStart;
    uCol = yStart;
    if (delta_x > 0)incx = 1;                            // 设置单步方向
    else if (delta_x == 0)incx = 0;                      // 垂直线
    else
    {
        incx = -1;
        delta_x = -delta_x;
    }
    if (delta_y > 0) incy = 1;
    else if (delta_y == 0)incy = 0;                      // 水平线
    else
    {
        incy = -1;
        delta_y = -delta_y;
    }
    if (delta_x > delta_y)distance = delta_x;            // 选取基本增量坐标轴
    else distance = delta_y;
    for (t = 0; t <= distance + 1; t++)                  // 画线输出
    {
        LCD_DrawPoint(uRow, uCol, _color);               // 画点
        xerr += delta_x ;
        yerr += delta_y ;
        if (xerr > distance)
        {
            xerr -= distance;
            uRow += incx;
        }
        if (yerr > distance)
        {
            yerr -= distance;
            uCol += incy;
        }
    }
}





/******************************************************************
 * 函数名: LCD_Circle
 * 功  能: 在指定位置画圆
 * 参  数: uint16_t Xpos     X坐标
 *          uint16_t Ypos     圆心Y坐标
 *          uint16_t Radius   半径
 *          uint16_t _color   颜色
 * 注  意: 无
 *****************************************************************/
void LCD_Circle(uint16_t Xpos, uint16_t Ypos, uint16_t Radius, uint16_t _color)
{
    int16_t mx = Xpos, my = Ypos, x = 0, y = Radius;
    int16_t d = 1 - Radius;
    while (y > x)
    {
        LCD_DrawPoint(x + mx, y + my, _color);
        LCD_DrawPoint(-x + mx, y + my, _color);
        LCD_DrawPoint(-x + mx, -y + my, _color);
        LCD_DrawPoint(x + mx, -y + my, _color);
        LCD_DrawPoint(y + mx, x + my, _color);
        LCD_DrawPoint(-y + mx, x + my, _color);
        LCD_DrawPoint(y + mx, -x + my, _color);
        LCD_DrawPoint(-y + mx, -x + my, _color);
        if (d < 0)
        {
            d += 2 * x + 3;
        }
        else
        {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}



/*****************************************************************
 * 函数名: LCD_Rectangle
 * 功  能: 画一个矩形 (参考野火代码)
 * 参  数: x       起始x坐标
 *         y       起始y坐标
 *         width   矩形的像素宽度
 *         height  矩形的像素高度
 *         color   颜色
 *         filled  是否实心填充
 * 返回值: 无
*****************************************************************/
void LCD_Rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color, uint8_t filled)
{
    if (filled)
    {
        LCD_Fill(x, y, width, height, color);
    }
    else
    {
        LCD_Line(x, y, x + width - 1, y, color);
        LCD_Line(x, y + height - 1, x + width - 1, y + height - 1, color);
        LCD_Line(x, y, x, y + height - 1, color);
        LCD_Line(x + width - 1, y, x + width - 1, y + height - 1, color);
    }
}



/******************************************************************
 * 函数名: LCD_Image
 * 功  能: 在指定区域显示指定图片数据
 * 注  意: 图片数据在font.h文件中.只适合小图片哦.
 *         Image2Lcd转换，水平扫描，16位真彩色, 低位在前(注意和原图颜色可能相反)
 * 参  数: uint16_t x,y     左上角起始坐标
 *          uint16_t width   图片宽度
 *          uint16_t height  图片高度
 *          uint8_t* image   数据缓冲地址
 * 返回值: 无
 *****************************************************************/
void LCD_Image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *image)
{
    for (uint16_t i = 0; i < height; i++)           // 一行一行的显示
    {
        LCD->LCD_REG = 0X2A;                        // 设置x坐标
        LCD->LCD_RAM = x >> 8;                      // X坐标的高8位
        LCD->LCD_RAM = x ;                          // X坐标的低8位，因为坐标值只是低8位有效，所以等效于 x1 & 0XFF
        LCD->LCD_REG = 0X2B;                        // 设置y坐标
        LCD->LCD_RAM = (y + i) >> 8;                // Y坐标的高8位
        LCD->LCD_RAM = y + i;                       // Y坐标的低8位，因为坐标值只是低8位有效，所以等效于 Y & 0XFF
        LCD->LCD_REG = 0X2C;                        // 开始写GRAM
        for (uint16_t j = 0; j < width; j++)        // 一列中，从左到右逐个像素传输
        {
            LCD->LCD_RAM = image[1] << 8 | *image;  // 写入16位颜色数据
            image += 2;                             // 偏移指针到下一组字节
        }
    }
}



/******************************************************************
 * 函数名: LCD_ShowChinese
 * 功  能: 显示直接取模的汉字,
 *         字库数据在font文件中，只适合少量汉字固定场合
 * 取模软件: 使用PCtoLCD2018 (显示效果的细节已被调节到最佳状态)
 *          1-选择字库
 *          2-参数设置：阴码+列行式+逆向+C51格式; 16进制; 勾选: 输出文件+索引格式+压缩格式; 行数值(字库x字高/8); 列数值(80)
 *          3-输入要生成的文本，然后生成字模
 *          4-复制到font.h文件的对应数组尾部; 注意格式上的错误;
 * 参  数: uint16_t  x         坐标x
 *          uint16_t  y         坐标y
 *          uint8_t   size      字号; 注意，要调用对应字号的数组, 如果没有，请新建一个并修改下面函数
 *          uint8_t   index     字模数组中的索引
 *          uint32_t  fColor    字体颜色
 *          uint32_t  bColor    背景颜色
 * 返回值:  无
 *****************************************************************/
void LCD_ShowChinese(uint8_t x, uint8_t y, uint8_t size, uint8_t index, uint32_t fColor, uint32_t bColor)
{
    uint8_t m, temp;
    uint8_t x0 = x, y0 = y;
    uint16_t size3 = (size / 8 + ((size % 8) ? 1 : 0)) * size; // 得到字体一个字符对应点阵集所占的字节数

    for (uint16_t i = 0; i < size3; i++)
    {
        if (size == 12)
        {
            temp = aFontChinese12[index][i];   // 调用12*12字体
        }
        else if (size == 16)
        {
            temp = aFontChinese16[index][i];   // 调用16*16字体
        }
        else if (size == 24)
        {
            temp = aFontChinese24[index][i];   // 调用24*24字体
        }
        else if (size == 32)
        {
            temp = aFontChinese32[index][i];   // 调用32*32字体
        }
        else
        {
            temp = aFontChinese12[index][i];   // 如果是非法情形，就调用12*12字体
        }
        for (m = 0; m < 8; m++)
        {
            if (temp & 0x01)
                LCD_DrawPoint(x, y, fColor);
            else
                LCD_DrawPoint(x, y, bColor);
            temp >>= 1;
            y++;
        }
        x++;
        if ((x - x0) == size)
        {
            x = x0;
            y0 = y0 + 8;
        }
        y = y0;
    }
}



/******************************************************************
 * 函数名: LCD_Cross
 * 功  能: 在指定坐标上画出十字线，用于校准触摸屏
 * 参  数: uint16_t x   —  十字线的中心点坐标x
 *          uint16_t y   —  十字线的中心点坐标y
 *          uint16_t len     十字线的像素长度
 *          uint32_t fColor  颜色
 * 返回值: 无
 * 注  意: 无
 *****************************************************************/
void LCD_Cross(uint16_t x, uint16_t y, uint16_t len, uint32_t fColor)
{
    uint16_t temp = len / 2;

    LCD_Line(x - temp, y, x + temp, y, fColor);
    LCD_Line(x, y - temp, x, y + temp, fColor);
}



/*****************************************************************
 * 函数名: LCD_ReadPoint
 * 功  能: 读取某一个点的像素颜色值数据
 * 参  数: x      x坐标
 *         y      y坐标
 * 返回值: 16位真彩色值
*****************************************************************/
uint16_t  LCD_ReadPoint(uint16_t  x, uint16_t  y)
{
    uint16_t  r = 0, g = 0, b = 0;
    if (x >= xLCD.width || y >= xLCD.height)return 0; // 超过了范围,直接返回
    setCursor(x, y);
    LCD->LCD_REG = 0X2E;                              // 发送读GRAM指令

    r = LCD->LCD_RAM;                                 // dummy Read

    delay_us(20);
    r = LCD->LCD_RAM;                                 // 实际像素颜色

    delay_us(20);
    b = LCD->LCD_RAM;
    g = r & 0XFF;                                     // 第一次读取的是RG的值,R在前,G在后,各占8位
    g <<= 8;

    return (((r >> 11) << 11) | ((g >> 10) << 5) | (b >> 11));
}





/******************************************************************
 * 函数名: LCD_DispFlush
 * 功  能: 在指定区域刷新指定数据
 * 注  意: 专用于快速刷新图片数据到屏幕、16位色低位在前(注意和图片显示可能相反);
 *          此函数来源于LVGL移植的函数disp_flush()，可高效刷屏。
 * 参  数: uint16_t   x        左上角起始X坐标
 *          uint16_t   y        左上角起始Y坐标
 *          uint16_t   width    宽度，每列有多少个16位数据; 其数值等于图片的宽
 *          uint16_t   height   高度，每行有多少个16位数据; 其数值等于图片的高
 *          uint16_t  *pData    数据地址
 * 返回值: 无
 *****************************************************************/
void LCD_DispFlush(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *pData)
{
    for (uint16_t nowY = y; nowY <= height; nowY++)       // 逐行显示
    {
        LCD->LCD_REG = 0X2A;                              // 发送设置X坐标的指令
        LCD->LCD_RAM = x >> 8;                            // X坐标的高8位
        LCD->LCD_RAM = x ;                                // X坐标的低8位，因为坐标值只是低8位有效，所以等效于 x & 0XFF
        LCD->LCD_REG = 0X2B;                              // 发送设置Y坐标的指令
        LCD->LCD_RAM = nowY >> 8;                         // Y坐标的高8位
        LCD->LCD_RAM = nowY ;                             // Y坐标的低8位，因为坐标值只是低8位有效，所以等效于 Y & 0XFF
        LCD->LCD_REG = 0X2C;                              // 指令：开始写入GRAM
        for (uint16_t nowX = x; nowX <= width; nowX++)    // 一列中，从左到右逐个像素输出
        {
            LCD->LCD_RAM = *pData++;                      // 写入每个点的16位颜色数据, RGB565值
        }
    }
}





