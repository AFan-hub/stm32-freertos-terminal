# CubeMX 配置单（STM32F407ZGT6 + 瑞士军刀配件）

芯片：STM32F407ZGTx

## 时钟（RCC / Clock Configuration）

- RCC：HSE = Crystal/Ceramic Resonator（正点原子板载 8 MHz 晶振）
- PLL：M=8, N=336, P=2, Q=7 → HCLK 168 MHz, APB1 42 MHz, APB2 84 MHz
- SYS：Debug = **Serial Wire**（不选会占用 SWD 引脚，下次烧录变砖）

## 外设

| 外设 | 配置 | 引脚 | 接到 |
|---|---|---|---|
| USART1 | Asynchronous 115200 8N1；DMA 添加 **USART1_TX**（Normal, Byte）；NVIC 打开 USART1 global interrupt（Day 6 CLI 接收用） | PA9 TX / PA10 RX | 板载 USB 转串口 → PC（CLI + 日志） |
| USART2 | Asynchronous 115200；DMA 添加 **USART2_TX**（Normal, Byte）；NVIC 打开 USART2 global interrupt | PA2 TX / PA3 RX | 杜邦线 → CH340G → 树莓派 USB（数据链路） |
| I2C1 | I2C, Fast Mode 400 kHz | PB6 SCL / PB7 SDA | SSD1306 OLED（0x3C）与 MPU6050（0x68）**共用一条总线** |
| ADC1 | Channel IN1, 单次转换, 软件触发, 144 cycles | PA1 | 光敏电阻模块 AO |
| GPIO | 板载 LED0，Output Push-Pull（探索者 PF9 / 阿童木看丝印） | — | Day 1 闪烁验收 |

## FreeRTOS（Middleware → FREERTOS，Interface = CMSIS_V2）

FreeRTOSConfig.h 修改：

```text
configTOTAL_HEAP_SIZE                 32768
configCHECK_FOR_STACK_OVERFLOW        2
configUSE_MUTEXES                     1
configUSE_TRACE_FACILITY              1
configGENERATE_RUN_TIME_STATS         1
configUSE_STATS_FORMATTING_FUNCTIONS  1
```

注意：

- 开启 FreeRTOS 后 CubeMX 会自动把 HAL 时基从 SysTick 改到 TIM6，**不要改回去**
- 不要在 CubeMX 图形界面里建任务——在 `App/app_main.c` 里用原生
  `xTaskCreate / xQueueCreate` 建（面试问的是原生 API）

## 工程设置

- 把 `App/`、`BSP/`、`Components/` 加入 source path 与 include path
- 业务代码不要写进 `main.c`

## Day 1 验收顺序

1. 生成工程，`while(1){ LED toggle; HAL_Delay(500); }` → LED 500 ms 闪烁
2. 换成两个 FreeRTOS 任务（手册 3.2 的 TaskA/TaskB）→ LED 闪烁 + 串口 1 Hz 输出
3. 连续稳定运行 5 分钟无复位

任何一步不稳定：先查时钟树和 `configTOTAL_HEAP_SIZE`，不要往下推进。
