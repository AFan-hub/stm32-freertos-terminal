#ifndef BSP_MPU6050_H
#define BSP_MPU6050_H

/* MPU6050 (GY-521) on I2C1: SCL=PB6, SDA=PB7, 7-bit address 0x68.
 * Do NOT plug anything into the board P3 OLED/camera socket: its camera
 * data lines occupy PB6/PB7 (see docs/explorer_v3_board_notes.md §2).
 */

/* Single ACK probe at the active address, for live wiggle-testing. */
int BSP_MPU6050_Ping(void);

/* Bridge/short matrix on PB6-PB9 (see .c for reading the output). */
void BSP_I2C1_BridgeTest(void);

/* Bit-bang WHO_AM_I transaction with per-phase ACK visibility. */
void BSP_I2C_RawProbe(void);

int BSP_MPU6050_Init(void);   /* self-configuring: sweeps clock x pins x addr */

/* Which pin map init settled on: 0 = PB6/7, 1 = PB8/9. */
int BSP_MPU6050_MapIsPB89(void);

/* Print every ACKing address on I2C1 (PB6/7) to both UARTs. */
void BSP_I2C1_ScanReport(void);

/* Same scan with I2C1 temporarily remapped to PB8/P9 (P9 row 1, the
 * on-board chip bus), then restored. Off-by-one-row wiring detector. */
void BSP_I2C1_ScanReportAlt(void);

/* Acceleration in g (±2g range) and die temperature in °C. */
int BSP_MPU6050_Read(float *ax, float *ay, float *az, float *temp_c);

#endif
