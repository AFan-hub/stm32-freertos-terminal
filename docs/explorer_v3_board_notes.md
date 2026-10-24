# 探索者 V3 板级笔记（源自《探索者V3 硬件参考手册_V1.0》）

> 只记与本项目相关的结论。页码对应手册印刷页。

## 0. 跳线帽总清单（用某功能前先查这里，手册 P39 注意事项 5）

| 跳线 | 作用 | 本项目设置 |
|---|---|---|
| **P10**（2 帽） | 板载 CH340C ↔ USART1(PA9/PA10) 的连接开关；P10 同时是 PA9/PA10 引出口 | **二选一**：帽_on = 板载 Type-C(USB_UART) 即 USART1 控制台（此时拔掉 PA9/PA10 上的杜邦线）；帽_off = PA9/PA10 自由外接（当前 dongle 方案）。**严禁两者同时** |
| **P4**（2 帽） | USART2(PA2/PA3) ↔ RS232(COM2)/RS485  transceiver 的连接开关；P4 同时是 PA2/PA3 引出口 | **帽_off**：USART2 保持自由，接 CH340G→树莓派（Day 15 数据链路） |
| P2（2 帽） | USART3(PB10/PB11) ↔ RS232(COM3)/ATK 座 U17 | off（都不用） |
| P5（2 帽） | PA11/PA12 ↔ USB_SLAVE 或 CAN | 保持原样（两者都不用） |
| P11（1 帽位） | TPAD/STM_ADC(PA5)/STM_DAC(PA4)/RV1 滑动端互联 | **无帽**（除非要用板载电位器：帽接 DAC-RV1 → RV1 电压进 PA4=ADC12_IN4） |
| P1 | PWM_DAC(PA3 经 RC 滤波)/音频入 | 无帽 |

## 1. 串口真相（手册 2.1.3 / 2.1.31）

- 板载 USB 串口芯片 = **CH340C**，Type-C 座 **USB_UART**（也是主板主要供电口）
- CH340C 与 USART1 **不是硬连**，靠 **P10 跳线帽**对接 → Day 1"串口无数据"的根因之一
- 一键下载电路用 CH340C 的 DTR/RTS 控制 BOOT0/RESET（flymcu 用；Keil+ST-Link SWD 不受影响）
- 首次上电 CH340C 建连会使 STM32 复位 2~3 次，**正常现象**（手册 P38 注意事项 1）
- USART2 = PA2/PA3，经 P4 可选挂 RS232/RS485；**PA2 与 ETH_MDIO 共用**（不用以太网时 PHY 的 MDIO 为高阻输入，实测 USART2 115200 正常）；**PA3 与 PWM_DAC 共用**（P1 的二阶 RC 滤波挂在上面，实测未劣化 115200 通信）
- USART3 = PB10/PB11（RS232 COM3 / ATK 座）

## 2. 显示（手册 2.1.6 / 2.1.22）

- TFTLCD 座挂 FSMC 总线；背光 **PB15**；触摸信号 PB2/PF11/PB1/PB0/PC13（本项目不用触摸）；LCD RESET 与板复位键共用
- **板载 P3"OLED/摄像头座"的 OLED 走 PD6/PD7（软件 I2C）**，且 P3 的摄像头数据线占用 **PB6/PB7**（OV_D3/OV_D0）→ 我们的硬件 I2C1(PB6/PB7) 外接 SSD1306 时**不要插 P3 座**，用杜邦线；P3 插着 OLED/摄像头模块时 PB6/PB7 会被占用冲突
- 白屏排查顺序：重插模块 → 串口看 LCD ID（手册 P39 注意事项 6）

## 3. I2C 总线分布（手册 2.1.12 / 2.1.15 / 2.1.28）

- **板载 I2C 总线 = PB8/PB9**：挂 24C02 EEPROM（A0-A2 接地 → 7bit 地址 0x50）、ST480MC 三轴磁力计、ES8388 音频编解码
- **我们的外设 I2C1 = PB6/PB7**：MPU6050（0x68）+ SSD1306（0x3C）用杜邦线接这里
- 原子不提倡用 STM32 硬件 I2C（"鸡肋"原话）——我们 Day 2 若遇 I2C 怪病，备选方案是软件模拟 I2C

## 4. ADC 资源三选一（手册 2.1.13 / 2.1.26）

| 来源 | 引脚 | 接线 |
|---|---|---|
| 外接光敏模块 AO（当前 ioc 已配） | **PA1 = ADC1_IN1** | VCC→3V3, GND→GND, AO→PA1 |
| 板载光敏二极管 LS1 | **PF7 = ADC3_IN5** | 零接线（改代码启用 ADC3） |
| 板载电位器 RV1（P11 加帽 DAC-RV1） | **PA4 = ADC12_IN4** | 零接线（加跳线帽） |

## 5. 其他可用外设速查

- LED：DS0 红=**PF9**（在用）、DS1 绿=PF10；PWR 蓝=电源指示
- 按键：KEY0=PE4、KEY1=PE3、KEY2=PE2（内部上拉、低有效）；KEY_UP=**PA0**（高有效，WKUP）→ Day 6 可做 CLI 之外的物理交互
- 蜂鸣器 PF8（高电平就响，**别碰 PF8**）
- 板载 SPI FLASH **25Q128**：SPI1=PB3/PB4/PB5、CS=PB14（与 NRF24L01 座共用 SPI1；PB3/PB4 是 JTAG 脚 → **只用 SWD 仿真**）→ 后续可做配置存储/黑匣子扩展
- 温湿度座 U19：单总线 PG9（与摄像头 OV_PWDN 共用）
- 红外接收头 PA8（与 OV_XCLK 共用）
- TF 卡 SDIO：PC8~PC12/PD2（与 P3 摄像头数据线冲突）
- 以太网 YT8512C RMII（占用 PA2/PA1/PC1/PG11/13/14/PC4/5/PA7/PD3 —— 与 USART2_TX 共 PA2，不用以太网即无冲突）

## 6. 电源（手册 2.1.29 / P38 注意事项 2）

- 供电口：USB_UART Type-C（主）、DC 座 6~15V、VOUT 排针
- USB 单口理论 500mA；**带 4.3 寸屏 + 多模块时建议独立 DC 电源或双 USB 口供电**——若出现随机复位/屏闪，先怀疑供电
- 3.3V 数字/模拟分开稳压（U11/U12）

## 7. Day 2 接线表（MPU6050 GY-521 + 光敏模块）

| 模块脚 | 接板子 |
|---|---|
| MPU6050 VCC | 3V3（VOUT 排针） |
| MPU6050 GND | GND |
| MPU6050 SCL | **PB6** |
| MPU6050 SDA | **PB7** |
| 光敏 VCC | 3V3 |
| 光敏 GND | GND |
| 光敏 AO | **PA1** |

检查：P10 按当前方案（帽 off + dongle 接 PA9/PA10）或切帽 on 用板载口；P4 帽 off；P11/P1 无帽。
