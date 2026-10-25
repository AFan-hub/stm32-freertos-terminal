# stm32_freertos_terminal

STM32F407ZGT6 + FreeRTOS multi-task data acquisition & system monitor terminal.

Sensor/ADC/I2C acquisition -> Queue -> filtering/threshold -> DMA+RingBuffer
UART to PC (CLI/log) and to a Raspberry Pi 5 (binary protocol), with task
heartbeats, software watchdog, stack watermarks, Flash-persisted config
(magic/version/CRC) and a serial CLI.

## Layout

- `App/` — tasks and app glue (`app_main.c` creates every FreeRTOS object)
- `BSP/` — board drivers (uart, i2c, oled, flash, mpu6050)
- `Components/` — reusable, hardware-free units: `ring_buffer`, `crc16`,
  `protocol`, `cli` (the same CRC/protocol algorithm as the Pi gateway)
- `docs/cubemx_setup.md` — exact CubeMX pin/clock/FreeRTOS configuration
- `docs/dev_log.md` — daily log (interview material)

## Progress

- [x] Day 0: repo, types, ring buffer, CRC16, protocol encoder, CubeMX sheet
- [x] Day 1: CubeMX project + five tasks + TFT(FSMC)/UART/DWT-delay BSP
- [x] Day 2: acquisition pipeline live: photoresistor (ADC1_IN1) as alarm
      channel + on-chip temperature (ADC1_IN16, VREFINT self-calibrated);
      SSD1306 on I2C1 as secondary display. MPU6050 module declared faulty
      by cross-test, driver kept for a future third channel
- [x] Day 3: UART DMA + Ring Buffer async TX (TxCplt -> task notification ->
      CommTask pump), interrupt-driven RX ring
- [x] Day 4: MonitorTask stall detection + stack high-water reports
- [x] Day 5: Flash config persistence (magic/version/CRC16, sector 8)
- [x] Day 6: serial CLI (help/status/task/config/set/save/reboot) +
      FreeRTOS run-time stats on TIM5
- [ ] Day 7: README polish + resource usage record + final cleanup
