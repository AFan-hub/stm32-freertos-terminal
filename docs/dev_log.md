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
