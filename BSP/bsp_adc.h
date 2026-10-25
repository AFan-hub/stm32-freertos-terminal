#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdint.h>

/* Photoresistor module AO -> PA1 = ADC1_IN1 (CubeMX configured).
 * Internal temperature sensor -> ADC1_IN16 (TSVREFE enabled in code).
 * On-board alternatives (docs/explorer_v3_board_notes.md §4):
 *   LS1 light diode  -> PF7 = ADC3_IN5 (no wiring)
 *   RV1 potentiometer-> PA4 = ADC12_IN4 (P11 jumper cap DAC-RV1)
 */

uint16_t BSP_ADC_ReadRaw(void);          /* 0..4095, light channel */
float    BSP_ADC_ReadLightPercent(void); /* 0..100 */
float    BSP_ADC_ReadTempC(void);        /* on-chip temperature, deg C */

#endif
