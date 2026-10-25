#include "bsp_mpu6050.h"

#include <stdio.h>

#include "bsp_delay.h"
#include "bsp_uart.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c1;

/* I2C1 pin mapping select.
 * 0 = PB6/PB7 (P9 row 2, the documented free bus)  <-- bench wiring
 * 1 = PB8/PB9 (P9 row 1, on-board chip bus)
 * Day-2 saga: a flaky dupont made the module appear on both nets at
 * different times; honest post-fix scans plus this macro settle it.
 */
#define BSP_I2C1_USE_PB89  0

#define BSP_I2C1_PINS()  (BSP_I2C1_USE_PB89 ? (GPIO_PIN_8 | GPIO_PIN_9) \
                                            : (GPIO_PIN_6 | GPIO_PIN_7))

static void i2c1_pins(uint32_t pins)
{
    GPIO_InitTypeDef g = { 0 };

    __HAL_RCC_GPIOB_CLK_ENABLE();
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    g.Pin       = pins;
    HAL_GPIO_Init(GPIOB, &g);
    bsp_delay_ms(2);
}

/* Some GY-521 clones ship with AD0 bridged -> address 0x69 instead of 0x68. */
#define MPU6050_ADDR_DEFAULT (0x68u << 1)
#define MPU6050_ADDR_ALT     (0x69u << 1)

static uint8_t s_mpu_addr = MPU6050_ADDR_DEFAULT;
static int     s_map = 0;   /* 0 = PB6/7, 1 = PB8/9 (whatever init found) */
#define MPU6050_REG_SMPLRT  0x19
#define MPU6050_REG_CONFIG  0x1A
#define MPU6050_REG_GYROCFG 0x1B
#define MPU6050_REG_ACCELCFG 0x1C
#define MPU6050_REG_PWR1    0x6B
#define MPU6050_REG_WHOAMI  0x75
#define MPU6050_REG_DATA    0x3B   /* 14 bytes: accel XYZ, temp, gyro XYZ */

#define MPU6050_TIMEOUT_MS  50

static int mpu_write_reg(uint8_t reg, uint8_t value)
{
    return (HAL_I2C_Mem_Write(&hi2c1, s_mpu_addr, reg,
                              I2C_MEMADD_SIZE_8BIT, &value, 1,
                              MPU6050_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}

static int mpu_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return (HAL_I2C_Mem_Read(&hi2c1, s_mpu_addr, reg,
                             I2C_MEMADD_SIZE_8BIT, buf, len,
                             MPU6050_TIMEOUT_MS) == HAL_OK) ? 0 : -1;
}

/* Day-2 diagnostic: report every device ACKing on I2C1 over both UARTs. */
static void i2c1_scan_print(const char *tag)
{
    char line[64];
    int n = snprintf(line, sizeof(line), "[i2c%s]:", tag);

    for (uint8_t addr = 0x08; addr < 0x78; ++addr)
    {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 1, 5) == HAL_OK)
        {
            n += snprintf(line + n, sizeof(line) - (size_t)n, " %02X", addr);
        }
    }

    if (line[n - 1] == ':')
    {
        n += snprintf(line + n, sizeof(line) - (size_t)n, " none");
    }

    snprintf(line + n, sizeof(line) - (size_t)n, "\r\n");
    BSP_UART1_Print(line);
    BSP_UART2_Print(line);
}

void BSP_I2C1_ScanReport(void)
{
    i2c1_pins(GPIO_PIN_6 | GPIO_PIN_7);   /* actually switch, not just label */
    i2c1_scan_print("@PB6/7");
    i2c1_pins(BSP_I2C1_PINS());
}

/* Second mapping probe: I2C1 can also live on PB8/PB9 (the on-board
 * EEPROM/magnetometer/codec bus, P9 row 1). If the MPU6050 wires sit one
 * row too high on P9, it shows up here together with 0x50 (24C02).
 * Restores PB6/PB7 afterwards.
 */
void BSP_I2C1_ScanReportAlt(void)
{
    i2c1_pins(GPIO_PIN_8 | GPIO_PIN_9);
    i2c1_scan_print("@PB8/9");
    i2c1_pins(BSP_I2C1_PINS());   /* restore the active mapping */
}

/* Raw bit-bang probe of the WHO_AM_I transaction with per-phase ACK
 * visibility (Day-2 forensics). Printed ack bits: 0 = ACK, 1 = NAK.
 *   aw = address+write ack, rg = register byte ack,
 *   ar = address+read ack (after repeated start), data = byte read.
 * Runs at ~50 kHz software timing on PB6/PB7, independent of the HAL.
 */
void BSP_I2C_RawProbe(void)
{
    const uint32_t scl = GPIO_PIN_6;
    const uint32_t sda = GPIO_PIN_7;
    GPIO_InitTypeDef g = { 0 };
    char line[64];
    int n;
    uint8_t data = 0;
    int aw, rg, ar;

#define SCL_H() (GPIOB->BSRR = scl)
#define SCL_L() (GPIOB->BSRR = (scl << 16))
#define SDA_H() (GPIOB->BSRR = sda)
#define SDA_L() (GPIOB->BSRR = (sda << 16))
#define SDA_R() (((GPIOB->IDR & sda) != 0u) ? 1 : 0)
#define T()     bsp_delay_us(10)

    __HAL_RCC_GPIOB_CLK_ENABLE();
    g.Mode  = GPIO_MODE_OUTPUT_OD;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin   = scl | sda;
    HAL_GPIO_Init(GPIOB, &g);

    SDA_H(); SCL_H(); T();          /* idle */
    SDA_L(); T(); SCL_L(); T();     /* start */

    /* address + write */
    for (int i = 7; i >= 0; --i)
    {
        if ((0xD0 >> i) & 1) { SDA_H(); } else { SDA_L(); }
        T(); SCL_H(); T(); SCL_L(); T();
    }
    SDA_H(); T(); SCL_H(); T(); aw = SDA_R(); SCL_L(); T();

    /* register byte 0x75 */
    for (int i = 7; i >= 0; --i)
    {
        if ((0x75 >> i) & 1) { SDA_H(); } else { SDA_L(); }
        T(); SCL_H(); T(); SCL_L(); T();
    }
    SDA_H(); T(); SCL_H(); T(); rg = SDA_R(); SCL_L(); T();

    /* repeated start */
    SDA_H(); T(); SCL_H(); T();
    SDA_L(); T(); SCL_L(); T();

    /* address + read */
    for (int i = 7; i >= 0; --i)
    {
        if ((0xD1 >> i) & 1) { SDA_H(); } else { SDA_L(); }
        T(); SCL_H(); T(); SCL_L(); T();
    }
    SDA_H(); T(); SCL_H(); T(); ar = SDA_R(); SCL_L(); T();

    /* one data byte, then NAK + stop */
    for (int i = 7; i >= 0; --i)
    {
        SDA_H(); T(); SCL_H(); T();
        data = (uint8_t)((data << 1) | (uint8_t)SDA_R());
        SCL_L(); T();
    }
    SDA_H(); T(); SCL_H(); T(); SCL_L(); T();   /* NAK */
    SDA_L(); T(); SCL_H(); T(); SDA_H(); T();   /* stop */

#undef SCL_H
#undef SCL_L
#undef SDA_H
#undef SDA_L
#undef SDA_R
#undef T

    n = snprintf(line, sizeof(line),
                 "[raw] aw:%d rg:%d ar:%d data:%02X\r\n", aw, rg, ar, data);
    BSP_UART1_SendBuffer((const uint8_t *)line, (uint16_t)n);
    BSP_UART2_SendBuffer((const uint8_t *)line, (uint16_t)n);

    i2c1_pins(BSP_I2C1_PINS());
}

int BSP_MPU6050_Ping(void)
{
    return (HAL_I2C_IsDeviceReady(&hi2c1, s_mpu_addr, 1, 5) == HAL_OK) ? 1 : 0;
}

/* Electrical bridge/short matrix on the four candidate I2C pins.
 * Idle: all four input+pullup, expect 1 (a 0 means stuck low: short to GND
 * or a device holding the line). Then each pin in turn drives low while
 * the others are sampled: expect 1 (a 0 means that pair is bridged).
 */
void BSP_I2C1_BridgeTest(void)
{
    static const uint16_t pins[4] =
        { GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_8, GPIO_PIN_9 };
    GPIO_InitTypeDef g = { 0 };
    char line[72];
    int n;

    __HAL_RCC_GPIOB_CLK_ENABLE();

    g.Mode  = GPIO_MODE_INPUT;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin   = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9;
    HAL_GPIO_Init(GPIOB, &g);
    bsp_delay_ms(2);

    n = snprintf(line, sizeof(line), "[bridge] idle 6:%d 7:%d 8:%d 9:%d\r\n",
                 (GPIOB->IDR & pins[0]) ? 1 : 0,
                 (GPIOB->IDR & pins[1]) ? 1 : 0,
                 (GPIOB->IDR & pins[2]) ? 1 : 0,
                 (GPIOB->IDR & pins[3]) ? 1 : 0);
    BSP_UART1_SendBuffer((const uint8_t *)line, (uint16_t)n);
    BSP_UART2_SendBuffer((const uint8_t *)line, (uint16_t)n);

    for (int d = 0; d < 4; ++d)
    {
        g.Mode = GPIO_MODE_OUTPUT_PP;
        g.Pull = GPIO_NOPULL;
        g.Pin  = pins[d];
        HAL_GPIO_Init(GPIOB, &g);
        HAL_GPIO_WritePin(GPIOB, pins[d], GPIO_PIN_RESET);
        bsp_delay_ms(1);

        n = snprintf(line, sizeof(line), "[bridge] PB%d low ->", 6 + d);
        for (int o = 0; o < 4; ++o)
        {
            if (o == d) continue;
            n += snprintf(line + n, sizeof(line) - (size_t)n, " %d:%d",
                          6 + o, (GPIOB->IDR & pins[o]) ? 1 : 0);
        }
        n += snprintf(line + n, sizeof(line) - (size_t)n, "\r\n");
        BSP_UART1_SendBuffer((const uint8_t *)line, (uint16_t)n);
        BSP_UART2_SendBuffer((const uint8_t *)line, (uint16_t)n);

        g.Mode = GPIO_MODE_INPUT;
        g.Pull = GPIO_PULLUP;
        g.Pin  = pins[d];
        HAL_GPIO_Init(GPIOB, &g);
        bsp_delay_ms(1);
    }

    i2c1_pins(BSP_I2C1_PINS());
}

int BSP_MPU6050_Init(void)
{
    uint8_t id = 0;
    int found = 0;
    int lasterr = -1;

    /* Self-configuring bring-up: the bench wiring's electrical row proved
     * unguessable across sessions, so sweep {clock} x {pin map} x {address}
     * and keep whichever combination answers WHO_AM_I correctly.
     *   -1xx  = WHO_AM_I read failed, xx = HAL status (1 err, 2 busy, 3 to)
     *   -2xx  = read OK but wrong id, xx = the id byte actually received
     *   -3/-4 = reset / config write failure (see below)
     */
    static const uint32_t clocks[2] = { 100000, 400000 };

    for (int c = 0; c < 2 && !found; ++c)
    {
        hi2c1.Init.ClockSpeed = clocks[c];
        HAL_I2C_Init(&hi2c1);

        for (int map = 0; map < 2 && !found; ++map)
        {
            i2c1_pins(map ? (GPIO_PIN_8 | GPIO_PIN_9)
                          : (GPIO_PIN_6 | GPIO_PIN_7));

            for (int try = 0; try < 2 && !found; ++try)
            {
                s_mpu_addr = (try == 0) ? MPU6050_ADDR_DEFAULT
                                        : MPU6050_ADDR_ALT;

                HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c1, s_mpu_addr,
                                                        MPU6050_REG_WHOAMI,
                                                        I2C_MEMADD_SIZE_8BIT,
                                                        &id, 1,
                                                        MPU6050_TIMEOUT_MS);
                if (st != HAL_OK)
                {
                    lasterr = -(100 + (int)st * 10 +
                                (int)HAL_I2C_GetError(&hi2c1));
                }
                else if (id != 0x68)
                {
                    lasterr = -200 - (int)id;
                }
                else
                {
                    s_map = map;
                    found = 1;
                }
            }
        }
    }

    if (!found)
    {
        s_mpu_addr = MPU6050_ADDR_DEFAULT;
        return lasterr;
    }

    if (mpu_write_reg(MPU6050_REG_PWR1, 0x80) != 0)   /* device reset */
    {
        return -2;
    }
    bsp_delay_ms(100);      /* datasheet: up to 100 ms after reset */

    if (mpu_write_reg(MPU6050_REG_PWR1, 0x00) != 0)   /* wake up */
    {
        return -3;
    }
    bsp_delay_ms(20);

    if (mpu_write_reg(MPU6050_REG_SMPLRT, 0x07) != 0 ||   /* 125 Hz ODR */
        mpu_write_reg(MPU6050_REG_CONFIG, 0x00) != 0 ||
        mpu_write_reg(MPU6050_REG_GYROCFG, 0x08) != 0 ||  /* ±500 dps */
        mpu_write_reg(MPU6050_REG_ACCELCFG, 0x00) != 0)   /* ±2 g */
    {
        return -4;
    }

    return 0;
}

int BSP_MPU6050_MapIsPB89(void)
{
    return s_map;
}

int BSP_MPU6050_Read(float *ax, float *ay, float *az, float *temp_c)
{
    uint8_t raw[14];
    int16_t v[7];

    if (mpu_read_regs(MPU6050_REG_DATA, raw, sizeof(raw)) != 0)
    {
        return -1;
    }

    for (int i = 0; i < 7; ++i)
    {
        v[i] = (int16_t)((uint16_t)(raw[2 * i] << 8) | raw[2 * i + 1]);
    }

    *ax = (float)v[0] / 16384.0f;          /* ±2 g */
    *ay = (float)v[1] / 16384.0f;
    *az = (float)v[2] / 16384.0f;
    *temp_c = (float)v[3] / 340.0f + 36.53f;

    return 0;
}
