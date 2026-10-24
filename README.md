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
- [ ] Day 1: CubeMX project + LED + two tasks (acceptance: stable 5 min)
- [ ] Day 2: SensorTask + SensorQueue + ProcessTask
- [ ] Day 3: Ring Buffer + UART DMA + TxCplt notification
- [ ] Day 4: DisplayTask + MonitorTask
- [ ] Day 5: heartbeat / stack watermark / fault flags / overflow hook
- [ ] Day 6: Flash config + CRC + ConfigMutex
- [ ] Day 7: CLI + README + resource usage record
