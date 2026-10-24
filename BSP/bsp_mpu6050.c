#include "bsp_mpu6050.h"

#include <stdio.h>

#include "bsp_delay.h"
#include "bsp_uart.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c1;

/* I2C1 pin mapping select.
 * 0 = PB6/PB7 (P9 row 2, the documented free bus)
 * 1 = PB8/PB9 (P9 row 1) -- current bench wiring sits one row high; the
 *     on-board chips do not ACK on this net (scan shows only the MPU),
 *     so sharing it is harmless here. Flip to 0 after moving the wires.
 */
#define BSP_I2C1_USE_PB89  1

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
    i2c1_scan_print("@PB6/7");
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

    i2c1_pins(BSP_I2C1_PINS());   /* enforce the selected mapping */

    /* address auto-detect: 0x68 first, AD0-bridged clones answer 0x69 */
    s_mpu_addr = MPU6050_ADDR_DEFAULT;
    if (mpu_read_regs(MPU6050_REG_WHOAMI, &id, 1) != 0 || id != 0x68)
    {
        s_mpu_addr = MPU6050_ADDR_ALT;
        if (mpu_read_regs(MPU6050_REG_WHOAMI, &id, 1) != 0 || id != 0x68)
        {
            s_mpu_addr = MPU6050_ADDR_DEFAULT;
            return -1;
        }
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
