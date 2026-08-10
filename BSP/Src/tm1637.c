/**
 ******************************************************************************
 * @file    tm1637.c
 * @brief   TM1637 6-digit 7-segment LED display driver (HAL version)
 * @note    Custom 2-wire protocol (similar to I2C)
 *          TM1637_CLK --> PB15
 *          TM1637_DIO --> PB14
 *
 *          The DIO pin is bi-directional: switches between Output PP and
 *          Input Floating depending on operation.
 ******************************************************************************
 */

#include "tm1637.h"
#include "delay.h"

/* 7-Segment Display Tables --------------------------------------------------*/
/* Digit patterns without decimal point */
static unsigned char table[]  = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};

/* Digit patterns with decimal point (MSB=1 adds DP) */
static unsigned char table1[] = {0xbf, 0x86, 0xdb, 0xcf, 0xe6, 0xed, 0xfd, 0x87, 0xff, 0xef};

/*============================================================================
 * DIO Direction Control
 *============================================================================*/

/**
 * @brief  Set TM1637 DIO pin as input (for reading ACK)
 * @note   Direct register operation for speed (avoids HAL_GPIO_Init overhead)
 *         PB14: CRH bits[24:27] = 0100 (CNF=01 floating, MODE=00 input)
 */
static void TM1637_DIO_SetInput(void)
{
    GPIOB->CRH &= ~(0xF << 24);
    GPIOB->CRH |=  (0x4 << 24);   /* CNF=01, MODE=00 → Input floating */
}

/**
 * @brief  Set TM1637 DIO pin as output (for writing data)
 * @note   Direct register operation for speed (avoids HAL_GPIO_Init overhead)
 *         PB14: CRH bits[24:27] = 0011 (CNF=00 PP, MODE=11 50MHz)
 */
static void TM1637_DIO_SetOutput(void)
{
    GPIOB->CRH &= ~(0xF << 24);
    GPIOB->CRH |=  (0x3 << 24);   /* CNF=00, MODE=11 → Output PP, 50MHz */
}

/*============================================================================
 * TM1637 Protocol Primitives
 *============================================================================*/

/**
 * @brief  Generate start condition for TM1637
 */
static void I2CStart(void)
{
    TM1637_DIO_SetOutput();

    TM1637_CLK_H;
    TM1637_DIO_H;
    delay_us(2);
    TM1637_DIO_L;
}

/**
 * @brief  Wait for ACK from TM1637
 */
static void I2Cask(void)
{
    TM1637_CLK_L;
    delay_us(5);  /* Wait 5us after 8th clock falling edge, then check ACK */
    TM1637_DIO_SetInput();
    while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14));  /* Wait for DIO low */
    TM1637_CLK_H;
    delay_us(2);
    TM1637_CLK_L;

    TM1637_DIO_SetOutput();
}

/**
 * @brief  Generate stop condition for TM1637
 */
static void I2CStop(void)
{
    TM1637_DIO_SetOutput();

    TM1637_CLK_L;
    delay_us(2);
    TM1637_DIO_L;
    delay_us(2);
    TM1637_CLK_H;
    delay_us(2);
    TM1637_DIO_H;
}

/**
 * @brief  Write one byte to TM1637 (LSB first)
 * @param  oneByte: byte to send
 */
static void I2CWrByte(unsigned char oneByte)
{
    unsigned char i;

    TM1637_DIO_SetOutput();

    for (i = 0; i < 8; i++)
    {
        TM1637_CLK_L;
        if (oneByte & 0x01)  /* Send LSB first */
        {
            TM1637_DIO_H;
        }
        else
        {
            TM1637_DIO_L;
        }
        delay_us(3);
        oneByte = oneByte >> 1;
        TM1637_CLK_H;
        delay_us(3);
    }
}

/*============================================================================
 * High-Level Functions
 *============================================================================*/

/**
 * @brief  Scan keys on TM1637 (returns key code)
 * @return Key code or 0 if no key pressed
 * @note   This function is provided for compatibility; TM1637 is
 *         display-only, key scan requires TM1638
 */
unsigned char ScanKey(void)
{
    unsigned char rekey = 0, i;

    I2CStart();
    I2CWrByte(0x42);   /* Read key command */
    I2Cask();
    TM1637_DIO_H;

    TM1637_DIO_SetInput();
    for (i = 0; i < 8; i++)  /* Read from LSB */
    {
        TM1637_CLK_L;
        rekey = rekey >> 1;
        delay_us(30);
        TM1637_CLK_H;
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14))
        {
            rekey = rekey | 0x80;
        }
        else
        {
            rekey = rekey | 0x00;
        }
        delay_us(30);
    }
    I2Cask();
    I2CStop();

    return (rekey);
}

/**
 * @brief  Display test pattern (digits 0-5) on TM1637
 */
void SmgDisplay(void)
{
    unsigned char i;

    I2CStart();
    I2CWrByte(0x40);   /* Auto-increment address mode */
    I2Cask();
    I2CStop();

    I2CStart();
    I2CWrByte(0xC0);   /* Start address */
    I2Cask();
    for (i = 0; i < 6; i++)  /* Auto-increment, no need to re-send address */
    {
        I2CWrByte(table[i]);
        I2Cask();
    }
    I2CStop();

    I2CStart();
    I2CWrByte(0x8A);   /* Display on, brightness level 8/8 */
    I2Cask();
    I2CStop();
}

/**
 * @brief  Display a voltage value on TM1637 (6 digits with 1 decimal point)
 * @param  dat: voltage * 100000 (e.g., 3300000 = 3.30000V displayed as "3.30000")
 * @note   First digit shown with decimal point
 *         The value dat/10 is split into 6 individual digits
 */
void TM1637_SHOW(unsigned long dat)
{
    unsigned char a, b, c, d, e, f;
    unsigned long tem;

    tem = dat / 10;

    a = (unsigned char)(tem % 1000000 / 100000);
    b = (unsigned char)(tem % 100000 / 10000);
    c = (unsigned char)(tem % 10000 / 1000);
    d = (unsigned char)(tem % 1000 / 100);
    e = (unsigned char)(tem % 100 / 10);
    f = (unsigned char)(tem % 10);

    I2CStart();
    I2CWrByte(0x40);   /* Auto-increment address mode */
    I2Cask();
    I2CStop();

    I2CStart();
    I2CWrByte(0xC0);   /* Start address */
    I2Cask();
    I2CWrByte(table1[a]);  /* First digit with decimal point */
    I2Cask();
    I2CWrByte(table[b]);
    I2Cask();
    I2CWrByte(table[c]);
    I2Cask();
    I2CWrByte(table[d]);
    I2Cask();
    I2CWrByte(table[e]);
    I2Cask();
    I2CWrByte(table[f]);
    I2Cask();

    I2CStop();

    I2CStart();
    I2CWrByte(0x8A);   /* Display on, brightness 8/8 */
    I2Cask();
    I2CStop();
}

/**
 * @brief  Display a single digit at specified position, blank others
 * @param  pos: digit position (0~5, left to right)
 * @param  num: number to display (0~9)
 * @param  dp: 1 = show decimal point, 0 = no decimal point
 */
void TM1637_ShowDigit(uint8_t pos, uint8_t num, uint8_t dp)
{
    uint8_t i;
    uint8_t seg_data;

    if (pos > 5) pos = 5;
    if (num > 9) num = 9;

    seg_data = dp ? table1[num] : table[num];

    I2CStart();
    I2CWrByte(0x40);   /* Auto-increment address mode */
    I2Cask();
    I2CStop();

    I2CStart();
    I2CWrByte(0xC0);   /* Start address */
    I2Cask();
    for (i = 0; i < 6; i++)
    {
        I2CWrByte((i == pos) ? seg_data : 0x00);  /* Only target position shows data */
        I2Cask();
    }
    I2CStop();

    I2CStart();
    I2CWrByte(0x8A);   /* Display on, brightness 8/8 */
    I2Cask();
    I2CStop();
}

/**
 * @brief  Display a number (0~99), auto-detect 1 or 2 digits, blank others
 * @param  start_pos: leftmost display position (0~5)
 * @param  num: value to display (0~99)
 * @note   1-digit (0~9):   show at start_pos, other 5 positions blanked
 *          2-digit (10~99): show tens at start_pos, ones at start_pos+1, others blanked
 */
void TM1637_ShowNum(uint8_t start_pos, uint8_t num)
{
    uint8_t i;
    uint8_t tens, ones;
    uint8_t seg[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    if (start_pos > 5) start_pos = 5;
    if (num > 99) num = 99;

    tens = num / 10;
    ones = num % 10;

    if (num < 10)
    {
        /* 1-digit: only ones position */
        seg[start_pos] = table[ones];
    }
    else
    {
        /* 2-digit: tens + ones */
        seg[start_pos]     = table[tens];
        if (start_pos < 5)
            seg[start_pos + 1] = table[ones];
    }

    I2CStart();
    I2CWrByte(0x40);   /* Auto-increment address mode */
    I2Cask();
    I2CStop();

    I2CStart();
    I2CWrByte(0xC0);   /* Start address */
    I2Cask();
    for (i = 0; i < 6; i++)
    {
        I2CWrByte(seg[i]);
        I2Cask();
    }
    I2CStop();

    I2CStart();
    I2CWrByte(0x8A);   /* Display on, brightness 8/8 */
    I2Cask();
    I2CStop();
}
