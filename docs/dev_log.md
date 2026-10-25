# 开发日志

> 每天结束前按模板补一节。面试时很多真实细节就来自这个日志。

## Day 0（2026-10-23）

今天完成：

1. 硬件清点：正点原子 F407ZGT6、树莓派 5（电源/SD 卡齐）、韦东山瑞士军刀配件（SSD1306 OLED、MPU6050、CH340G、ST-Link、光敏电阻、杜邦线）
2. 项目 2 骨架生成并通过宿主机单测：CRC16（0x29B1 向量）、Protocol 编解码、FrameParser 六用例（完整帧/半包/粘包/垃圾前缀/CRC 错恢复/LEN 上限重同步）
3. 项目 1：CubeMX 配置单（docs/cubemx_setup.md）、app_types.h、ring_buffer、crc16、protocol C 端

今天发现的问题：

1. C++ 端 `sizeof(SensorPayload)` = 16（对齐尾填充），C 端 packed = 14 —— 直接用 sizeof 当线上长度会导致两端永远对不上
2. WSL Ubuntu-22.04 没装 C++ 工具链

解决方法：

1. 线上长度钉死为常量 `SENSOR_PAYLOAD_SIZE = 14`，两端注释互指；单测当天抓住，没流到联调
2. 待装：`sudo apt install -y build-essential cmake`；或直接在树莓派上 Remote-SSH 开发

明天第一件事：

1. 项目 1 Day 1：按 docs/cubemx_setup.md 建 CubeMX 工程，LED 闪烁验收
2. 通读 FrameParser.cpp / ThreadSafeQueue.hpp，能口述状态机和 predicate 的作用

## Day 0 续（2026-10-23 晚）

今天完成：

1. 仓库整体迁移到 `E:\embedded-projects`（以后所有代码在 E 盘）
2. 修复编译警告：项目 2 的 `namespace log` 与 GCC 内建数学函数 `log()` 撞名，触发 `-Wbuiltin-declaration-mismatch`；改名 `namespace logging`（5 个文件）
3. WSL 强制重编验证：warning 计数 0，ctest 2/2 通过

今天发现的问题：

1. 命名空间起名不能撞编译器内建函数名（log/exp/sqrt 等都是雷区）

解决方法：

1. 改名 `logging`；以后命名空间/宏命名避开 C 标准库函数名

明天第一件事：

1. 项目 1 Day 1：CubeMX 建工程 + LED 验收（若尚未完成）

## Day 1（2026-10-24）验收通过

今天完成：

1. 五任务 FreeRTOS 骨架 + TFT(FSMC 移植原子驱动)/UART/DWT delay BSP 全部跑通
2. 屏：竖屏 UI，UP 计数、C/P 心跳计数走动、F:0
3. 串口：banner + `[diag] uwTick delta/500ms=500 CR1=0x01 SR=0x00 DIER=0x01 NVIC=1` + 每秒 `[comm]` 行（经 USART2 + CH340G 模块通道）

今天发现的问题：

1. **看门狗误报**：ProcessTask 用 portMAX_DELAY 阻塞在空队列上，心跳永远不增 → MonitorTask 报 SYS_TASK_STALL（F:16）。阻塞等待型任务的心跳必须放在等待之外
2. **串口监听错口**：XCOM 开的 COM7 是裸 CH340G 模块（未接板子），板载串口的实际归属 USART 未确认；连查数轮才用"USART2 镜像 + 屏上心跳计数"定罪
3. **Keil uvprojx 覆盖事故**（两次）：Keil 开着时把内存里的旧工程写回磁盘，覆盖外部修改 → 规矩：改 uvprojx 前必须关 Keil；或改用 header-only / .c 内嵌 include 规避工程条目依赖
4. 启动窗口内 HAL_Delay 曾卡死（lcd_init 内），改用 DWT 忙等后消失；调度器启动后 diag 证明时基正常（delta=500），根因未再追，归档

解决方法：

1. ProcessTask 改 100ms 有界等待 + 每轮心跳
2. 日志双通道镜像（USART1+USART2）；板载 CH340 实际接哪个 USART 待查原理图
3. 见上
4. BSP/bsp_delay.h（DWT 周期计数忙等，header-only）

明天第一件事：

1. Day 2：SensorTask 接真实采集（MPU6050 I2C + 光敏 ADC）入 SensorQueue，ProcessTask 滤波+阈值
2. 查原理图确认板载 CH340 接到哪个 USART（资料盘→硬件资料→原理图，搜 CH340）

### 补充（当晚销案）

串口根因确认：**用 CH340G 模块监听时一直没把模块 RXD/TXD 接到 PA9/PA10（USART1）**，固件串口路径从一开始就是好的。教训：物理链路先于软件怀疑。USART2 镜像通道保留作调试通道（Day 15 接树莓派时收回，USART2 回归纯数据链路）。调试器中启动窗口 HAL_Delay  dwell 归档不复追（DWT 延时已使其不影响启动）。原理图查 CH340 任务取消。

## Day 2（2026-10-25）验收通过

今天完成：

1. MPU6050 I2C 驱动（WHO_AMI/复位/量程/14 字节突发读）+ 光敏 ADC 采集入 SensorQueue（满队丢样并置 SYS_QUEUE_OVERFLOW）
2. ProcessTask 一阶 IIR 滤波 + ConfigMutex 保护下读阈值 + alarm_flags；Display/Comm 消费镜像队列
3. 屏显 T/L/ALARM；串口行含 t/l/alm/png
4. 验收现象：T 从室温爬升过 30.0 触发 ALARM，L 遮光变化，F:0，png=1

今天发现的问题（I2C 接线悬案全过程，面试素材）：

1. 症状序列：scan none → scan 68 但 init 失败 → 双映射 scan 都见 68 → 最终 `1A 50 68` 双映射全同
2. 根因链：MPU 的 SCL/SDA 插在 P9 **第一行**（PB9/PB8，差一行 2.54mm）；后续重插时杜邦母头跨搭使 **PB6/7 与 PB8/9 两组脚物理桥接**（板载 EEPROM 0x50 因此两条映射都可见）
3. 处置：固件宏 `BSP_I2C1_USE_PB89=1` 把 I2C1 映射到 PB8/PB9 匹配现接线（该净线上实测仅 MPU+被桥接过来的板载芯片，地址互不冲突）；桥接点待 [bridge] 矩阵存档
4. 方法论沉淀：**地址扫描 + 双映射扫描 + 桥接矩阵 + 每秒 ping** 四层诊断，把"玄学接触问题"变成可读输出；复位后 100ms 等待是 MPU6050 datasheet 明写的坑

明天第一件事：

1. Day 3：UART DMA + Ring Buffer 异步日志（CommTask 去掉阻塞发送），验收=高频日志下 SensorTask 周期不抖
2. 讨要/存档 [bridge] 五行矩阵，定位 PB6/7-PB8/9 桥接点

## Day 2 续（I2C 悬案结案）

结论：**MPU6050 模块本体故障（或其排针焊点开裂）**。交叉验证：同线束同引脚接已知良好的 SSD1306 → `scan: 3C`（总线/线束/驱动全健康）；换回 MPU → 仅间歇地址 ACK、寄存器读全灭（AF, -114）。此前数分钟的正常工作 = 临界接触恰好接通，"成功但不可复现"即接触故障的症状。

复盘：第一次"成功→失败"振荡出现时就应直接用已知好器件交叉验证，而非先穷举固件假设（时序/地址/映射/速度），决定性实验晚了两轮。教训：**间歇性故障在排除所有确定性因素后，立刻用已知好部件对半切故障域**。固件侧改动均保留且有独立价值：复位后 100ms（datasheet）、100kHz 标准模式（长线裕量）、诊断四件套（双映射扫描/桥接矩阵/ping/错误码）。

待办：补焊或更换 MPU 模块；到货即插即验收 Day 2。等待期间推进 Day 3（不依赖 MPU）。

## Day 3-6 一次性完成（2026-10-25）

按用户要求不再按天推进，一次补齐项目 1 剩余全部模块：

1. **UART DMA + Ring Buffer 异步发送**（bsp_uart 重写）：业务写环缓冲 → DMA 发当前连续段 → TxCplt ISR 消费+`vTaskNotifyGiveFromISR` → CommTask 泵下一段；USART1/USART2 双通道；USART1 RX 中断+环缓冲供 CLI
2. **Flash 配置持久化**（bsp_flash）：sector 8，magic/version/CRC16 + 回读校验；启动加载失败回退默认；仅 `save` 命令写
3. **CLI**（Components/cli）：help/status/task/config/set period/set threshold/save/reboot；`task` 附 FreeRTOS run-time stats（TIM5 1MHz 自由计数）
4. **CommTask 重写**：泵 DMA + 排空 comm 队列（PC 日志行 + USART2 二进制协议帧 10Hz）+ CLI 行装配
5. **MonitorTask**：每 10s 打印各任务栈高水位
6. **事件组**：gSystemEventGroup 与 fault 位图同步置位

已知遗留：MPU6050 模块故障未换，`fault` 常驻 bit1（SYS_SENSOR_TIMEOUT）属预期，换模块后自愈。

### 资源档案（Keil Build Output, F407ZGT6 = 1MB Flash / 192KB RAM）

```
Program Size: Code=54224 RO-data=13476 RW-data=212 ZI-data=42068
Flash = Code+RO+RW = 67912 B  ≈ 66.3 KB = 6.5%
RAM   = RW+ZI      = 42280 B  ≈ 41.3 KB = 21.5%
```

ZI 大头：FreeRTOS 堆 32KB + TX/RX 环缓冲 1.75KB + HAL/队列控制块。
运行时栈余量 59%~86%（task 命令实测），CPU 占用 IDLE 95%。

## MPU 跳过决议与通道重排（2026-10-25）

依据手册 §8"如果暂时没有传感器，用 ADC 即可，不要因为传感器驱动卡住整个项目"，
跳过 MPU6050（模块故障待换），通道重排：

- sensor_1 = 光敏（PA1, ADC1_IN1）→ 阈值/报警主通道，默认阈值 70%（手电筒演示）
- sensor_2 = 片内温度（ADC1_IN16, TSVREFE 代码使能）→ 零硬件第二通道
- I2C 真实外设改由 SSD1306 OLED 承担（PB6/PB7，副屏显示 L/T/ALARM）
- 配置版本升 v2：旧 Flash 存档（v1, threshold=45）自动校验失败回退默认
- MPU 驱动保留在树中，未来可作为第三通道接回；SYS_SENSOR_TIMEOUT 不再由 MPU 触发

## 项目 1 结案（2026-10-25）

- 温度案终审：H1 成立——参考链 cal=1500 / r17=1502 / vdda=3295mV 全正确，
  r16=984 → die 12.5°C，与"室内 10 出头 + 少量自热"自洽；读数方向经手指加热
  常识判断正确。片内传感器精度 ±3~5°C 属器件等级，接受
- 光照通道全绿：三态测试（捂 <15 / 环境 40~70 / 强光触发 ALARM）；PA1 悬案=
  杜邦线未入孔，换线解决
- OLED 副屏 I2C 在线；fault=0x00 常驻；CLI/Flash 持久化/DMA/看门狗全验收
- 资源：Flash 6.5%、RAM 21.5%、IDLE 95%、栈余量 59~86%
- 欠账清单：MPU6050 模块到货后作第三通道接回（SensorTask 加一个采集源）；
  Day 7 收尾项：架构图入 README、代码质量自查表

## Day 0 续 2（2026-10-23 夜）

今天完成：

1. 冒烟测试脚本入库：`rpi_edge_gateway/scripts/smoke_test.sh`（socat 虚拟串口对 + 模拟器 + 网关，单会话内完成；WSL 虚拟机在会话间回收，跨命令编排必死）
2. 基准结果（seed=7, 200 帧, 2% 注错）：`frames_ok=196 crc_errors=4 resyncs=0 processed=196`，与 RNG 抽样复现预测的 4 帧注错精确一致

今天发现的问题：

1. **serial_simulator.py 的 --corrupt 原先在计算 CRC 之前翻转 payload 字节** → CRC 随之重算，帧完全合法，接收端永远发现不了。注错工具本身是坏的，CRC 错误路径实际只被单测 case 5 覆盖
2. 用户环境中曾出现 `frames_ok=288 crc_errors=12 resyncs=3`（共 300 帧）：系网关实例跨多次模拟器运行累计 + 早期 pts 接线混乱造成的真实链路损伤；错误恢复路径表现正确
3. `pkill -f <name>` 会匹配到执行 pkill 的 shell 自身命令行（自杀陷阱），用 `[n]ame` 正则规避
4. Git Bash 调 wsl 时 `/mnt/e` 会被 MSYS 路径转换改写成 Windows 路径，需 `MSYS_NO_PATHCONV=1`

解决方法：

1. 改为先组帧、再翻转 payload 区字节（`scripts/serial_simulator.py`），重跑基准 crc_errors=4 符合预期
2. 冒烟测试固定走 smoke_test.sh，不再手工拼 pts 编号
3. 见上
4. 见上

明天第一件事：

1. 项目 1 Day 1：CubeMX 建工程 + LED 验收
