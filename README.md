# stm32_pi — STM32F407 + FreeRTOS 多任务数据采集与系统监控终端

正点原子探索者 V3（STM32F407ZGT6）+ FreeRTOS：光敏/片内温度双通道采集 →
队列 → 滤波/阈值 → TFT 显示 + USART2 二进制协议帧（对接树莓派网关），
并具备任务心跳/软件看门狗、栈水位监控、Flash 配置持久化（CRC）、
串口 CLI 与 UART DMA + Ring Buffer 异步日志。

## 架构

```text
 ADC1_IN1 (光敏 PA1) ─┐
 ADC1_IN16 (片内温度) ─┤ SensorTask (10Hz, 优先级4)
                       │  xQueueSend(16)
                       ▼
                 gSensorQueue ──► ProcessTask (阻塞等待, 优先级3)
                                    │ 一阶IIR 0.8/0.2 + 阈值(ConfigMutex)
                                    ├──► gDisplayQueue ──► DisplayTask
                                    │      (TFT 竖屏 + SSD1306 副屏, 优先级1)
                                    └──► gCommQueue ────► CommTask (优先级2)
                                           │ 日志行 → USART1 (DMA+Ring)
                                           │ 协议帧 → USART2 (DMA+Ring)
                                           └─ CLI 解析 (USART1 中断 + RX环)

 MonitorTask (1s): 心跳(双缺失判死) / 栈高水位 / 故障事件组
 USART1 DMA TxCplt ISR → vTaskNotifyGiveFromISR → CommTask 泵下一段
```

## 任务与对象

| Task | 优先级 | 周期 | 职责 |
|---|---:|---|---|
| SensorTask | 4 | 100 ms (CLI 可调) | 采集→SensorQueue，满队丢样并置故障位 |
| ProcessTask | 3 | 队列驱动 | IIR 滤波、阈值判断、分发 Display/Comm |
| CommTask | 2 | 100 ms 泵 | UART DMA 泵、日志、协议帧、CLI |
| DisplayTask | 1 | 500 ms | TFT + OLED 显示 |
| MonitorTask | 4 | 1 s | 软件看门狗、栈水位、运行统计 |

Queues: SensorQueue(16) / DisplayQueue(8) / CommQueue(8)；
Mutex: ConfigMutex；EventGroup: 故障位镜像；Notification: DMA 完成。

## 二进制协议（与树莓派网关一致）

```
AA 55 VER CMD LEN_L LEN_H PAYLOAD CRC_L CRC_H     (小端, CRC16-CCITT-FALSE, 覆盖 VER..PAYLOAD)
```

| CMD | 名称 | 说明 |
|---|---|---|
| 0x01 | SENSOR_DATA | `uint32 ts_ms; float s1; float s2; uint16 status`（14 字节，小端） |
| 0x06 | HEARTBEAT | 预留 |

校验向量：`CRC16("123456789") == 0x29B1`（Pi 端单测同向量）。

## 串口与接线

| 端口 | 用途 | 接线 |
|---|---|---|
| USART1 (PA9/PA10) | CLI + 日志 | 板载 P10 跳线帽 + USB_UART Type-C |
| USART2 (PA2/PA3) | 协议帧 → Pi | 杜邦线 → CH340G → 树莓派 `/dev/ttyUSB0`；**P4 帽必须拔掉** |

## 构建 / 烧录

1. Keil：`MDK-ARM/stm32_pi.uvprojx`（只用 SWD 仿真；JTAG 引脚被板载外设占用）
2. Rebuild all → ST-Link 烧录
3. 注意：Keil 会就地生成对象文件，`.gitignore` 已排除，只跟踪工程文件

## CLI（USART1, 115200）

```
help | status | task | config | set period <ms> | set threshold <f> | save | reboot
```

`task` 额外输出 FreeRTOS run-time stats（TIM5 1MHz 自由计数）。

## 实测数据（详见 docs/dev_log.md）

- 资源：Flash **66.3 KB (6.5%)**，RAM **41.3 KB (21.5%)**（1MB/192KB）
- 运行时：IDLE 95%，栈余量 59%~86%，UART 环缓冲零丢包
- 稳定性：连续运行 5 分钟以上；拔掉光敏/温度通道故障位正确置位
- 与 Pi 联调：10 Hz 帧流，1751 帧零 CRC 错（Pi 侧记录）

## 目录

```
App/        tasks + app_main（对象创建/心跳/故障）
BSP/        uart(DMA+Ring) / adc / oled / lcd(FSMC, 原子驱动) / flash / mpu6050(+诊断) / delay(DWT)
Components/ ring_buffer / crc16 / protocol / cli   （硬件无关，PC 可单测）
docs/       dev_log.md（开发日志/踩坑记录）· cubemx_setup.md · explorer_v3_board_notes.md · code_review.md
```

## 已知限制

- MPU6050 模块故障（交叉验证判定）暂未接回；驱动与诊断工具保留，可作第三通道
- 未实现 IWDG 硬件看门狗（软件看门狗已覆盖任务级卡死）
- 100 Hz 加速压测未执行（10 Hz 基线 + 联调记录为准）
