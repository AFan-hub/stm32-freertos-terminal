#include "bsp_oled.h"

#include <stdio.h>
#include <string.h>

#include "bsp_oled_font.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c1;

#define OLED_ADDR_WR   0x78
#define OLED_CMD_BURST 32
#define OLED_TIMEOUT   50

static int oled_cmd(uint8_t cmd)
{
    uint8_t buf[2] = { 0x00, cmd };   /* Co=0, D/C#=0 -> command */
    return (HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR_WR, buf, 2,
                                    OLED_TIMEOUT) == HAL_OK) ? 0 : -1;
}

static int oled_data(const uint8_t *data, uint16_t len)
{
    uint8_t buf[OLED_CMD_BURST + 1];
    uint16_t off = 0;

    while (off < len)
    {
        uint16_t chunk = (len - off > OLED_CMD_BURST) ? OLED_CMD_BURST
                                                      : (len - off);
        buf[0] = 0x40;                /* Co=0, D/C#=1 -> data */
        memcpy(&buf[1], &data[off], chunk);

        if (HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR_WR, buf,
                                    (uint16_t)(chunk + 1),
                                    OLED_TIMEOUT) != HAL_OK)
        {
            return -1;
        }
        off = (uint16_t)(off + chunk);
    }
    return 0;
}

static int oled_set_pos(uint8_t page, uint8_t col)
{
    if (oled_cmd((uint8_t)(0xB0 | (page & 0x07))) != 0) return -1;
    if (oled_cmd((uint8_t)(0x00 | (col & 0x0F))) != 0)  return -1;
    return oled_cmd((uint8_t)(0x10 | ((col >> 4) & 0x0F)));
}

int BSP_OLED_Init(void)
{
    static const uint8_t seq[] = {
        0xAE,       /* display off            */
        0xD5, 0x80, /* clock div              */
        0xA8, 0x3F, /* mux ratio 64           */
        0xD3, 0x00, /* display offset 0       */
        0x40,       /* start line 0           */
        0x8D, 0x14, /* charge pump on         */
        0x20, 0x02, /* page addressing mode   */
        0xA1,       /* segment remap          */
        0xC8,       /* COM scan dec           */
        0xDA, 0x12, /* COM pins config        */
        0x81, 0xCF, /* contrast               */
        0xD9, 0xF1, /* pre-charge             */
        0xDB, 0x40, /* VCOMH deselect         */
        0xA4,       /* resume to RAM content  */
        0xA6,       /* non-inverted           */
        0xAF,       /* display on             */
    };

    for (uint16_t i = 0; i < sizeof(seq); ++i)
    {
        if (oled_cmd(seq[i]) != 0)
        {
            return -1;
        }
    }

    BSP_OLED_Clear();
    return 0;
}

void BSP_OLED_Clear(void)
{
    uint8_t zeros[128] = { 0 };

    for (uint8_t page = 0; page < 8; ++page)
    {
        if (oled_set_pos(page, 0) != 0) return;
        oled_data(zeros, sizeof(zeros));
    }
}

void BSP_OLED_ShowString(uint8_t page, uint8_t col, const char *str)
{
    uint8_t cols[6];

    if (oled_set_pos(page, col) != 0) return;

    for (const char *p = str; *p != '\0'; ++p)
    {
        if (col > 126) break;

        char c = (*p >= 0x20 && *p <= 0x7F) ? *p : ' ';
        memcpy(cols, OLED_FONT[(uint8_t)(c - 0x20)], 5);
        cols[5] = 0x00;               /* inter-character gap column */

        if (oled_data(cols, 6) != 0) return;
        col = (uint8_t)(col + 6);
    }
}

void BSP_OLED_ShowUnsigned(uint8_t page, uint8_t col, uint32_t value)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)value);
    BSP_OLED_ShowString(page, col, buf);
}

void BSP_OLED_DrawBlock(uint8_t page, uint8_t col, uint8_t filled)
{
    uint8_t cols[6];
    uint8_t v = filled ? 0xFF : 0x00;

    memset(cols, v, 5);
    cols[5] = 0x00;

    if (oled_set_pos(page, col) != 0) return;
    oled_data(cols, 6);
}
