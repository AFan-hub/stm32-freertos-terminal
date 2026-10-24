#ifndef BSP_OLED_H
#define BSP_OLED_H

#include <stdint.h>

/* SSD1306 128x64, I2C1 (PB6/PB7), 7-bit address 0x3C (write 0x78).
 * Page addressing: 8 pages of 8 rows, 128 columns.
 */

int  BSP_OLED_Init(void);           /* 0 = panel acknowledged */
void BSP_OLED_Clear(void);
void BSP_OLED_ShowString(uint8_t page, uint8_t col, const char *str);
void BSP_OLED_ShowUnsigned(uint8_t page, uint8_t col, uint32_t value);
void BSP_OLED_DrawBlock(uint8_t page, uint8_t col, uint8_t filled);

#endif
